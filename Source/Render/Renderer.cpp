#include "Render/Renderer.h"
#include "Render/RenderCamera.h"
#include "Render/RenderFrame.h"

#pragma warning(push, 3)
#include <d3dcompiler.h>
#pragma warning(pop)

#include <cstring>

namespace de {

Renderer::~Renderer() {
    shutdown();
}

// -- Inline HLSL (instanced) --------------------------------------------

static const char k_shader_src[] = R"(
cbuffer OrthoMatrix : register(b0) {
    float4x4 ortho;
};

// Per-vertex: local quad position (slot 0).
struct VSIn {
    float2 local_pos : POSITION;
    // Per-instance data (slot 1).
    float2 world_pos : INST_POS;
    float  half_size : INST_SIZE;
    float4 color     : INST_COLOR;
};

struct PSIn {
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PSIn VSMain(VSIn i) {
    PSIn o;
    float2 wp = i.world_pos + i.local_pos * i.half_size;
    o.pos = mul(ortho, float4(wp, 0.0, 1.0));
    o.col = i.color;
    return o;
}

float4 PSMain(PSIn i) : SV_TARGET { return i.col; }
)";

// -- Instance data layout -----------------------------------------------

struct InstanceData {
    float pos_x;
    float pos_y;
    float half_size;
    float r, g, b, a;
};

static constexpr float k_half_size = 0.3f;

static constexpr float k_team_colors[][4] = {
    { 0.9f, 0.2f, 0.2f, 1.0f },   // team 0: red
    { 0.2f, 0.4f, 0.9f, 1.0f },   // team 1: blue
    { 0.2f, 0.8f, 0.3f, 1.0f },   // team 2: green
    { 0.9f, 0.8f, 0.1f, 1.0f },   // team 3: yellow
};
static constexpr uint32_t k_team_color_count = 4;

// -- Quad geometry ------------------------------------------------------

struct QuadVertex {
    float x, y;
};

// Unit quad: corners at (-1,-1) to (+1,+1), scaled by half_size in shader.
static constexpr QuadVertex k_quad_verts[] = {
    { -1.0f,  1.0f },  // TL
    {  1.0f,  1.0f },  // TR
    {  1.0f, -1.0f },  // BR
    { -1.0f, -1.0f },  // BL
};
static constexpr uint16_t k_quad_indices[] = {
    0, 1, 3,   // TL, TR, BL
    1, 2, 3,   // TR, BR, BL
};

// -- Device creation ----------------------------------------------------

bool Renderer::create_device() {
#ifdef _DEBUG
    {
        ComPtr<ID3D12Debug> dbg;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
            dbg->EnableDebugLayer();
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    ComPtr<IDXGIAdapter1> adapter;
    bool found = false;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
            found = true;
            break;
        }
    }

    if (!found) {
        if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
            return false;
        if (FAILED(D3D12CreateDevice(adapter.Get(),
                D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))))
            return false;
    }

    return true;
}

// -- Pipeline (root sig + shaders + PSO) --------------------------------

bool Renderer::create_pipeline() {
    UINT flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vs_blob, ps_blob, err;

    if (FAILED(D3DCompile(k_shader_src, sizeof(k_shader_src), "crowd.hlsl",
            nullptr, nullptr, "VSMain", "vs_5_0", flags, 0, &vs_blob, &err)))
        return false;
    if (FAILED(D3DCompile(k_shader_src, sizeof(k_shader_src), "crowd.hlsl",
            nullptr, nullptr, "PSMain", "ps_5_0", flags, 0, &ps_blob, &err)))
        return false;

    // Root signature: 16 root constants (4x4 ortho matrix at b0).
    D3D12_ROOT_PARAMETER rp = {};
    rp.ParameterType                = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp.Constants.ShaderRegister     = 0;
    rp.Constants.Num32BitValues     = 16;
    rp.ShaderVisibility             = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsd = {};
    rsd.NumParameters = 1;
    rsd.pParameters   = &rp;
    rsd.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rs_blob;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1,
            &rs_blob, &err)))
        return false;
    if (FAILED(device_->CreateRootSignature(0, rs_blob->GetBufferPointer(),
            rs_blob->GetBufferSize(), IID_PPV_ARGS(&root_sig_))))
        return false;

    // Input layout: slot 0 = per-vertex quad, slot 1 = per-instance data.
    D3D12_INPUT_ELEMENT_DESC il[] = {
        // Slot 0: per-vertex.
        { "POSITION",   0, DXGI_FORMAT_R32G32_FLOAT,       0,  0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        // Slot 1: per-instance.
        { "INST_POS",   0, DXGI_FORMAT_R32G32_FLOAT,       1,  0,
          D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SIZE",  0, DXGI_FORMAT_R32_FLOAT,          1,  8,
          D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 12,
          D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // PSO.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature                         = root_sig_.Get();
    pd.VS                                     = { vs_blob->GetBufferPointer(), vs_blob->GetBufferSize() };
    pd.PS                                     = { ps_blob->GetBufferPointer(), ps_blob->GetBufferSize() };
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask                             = UINT_MAX;
    pd.RasterizerState.FillMode               = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode               = D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable        = TRUE;
    pd.InputLayout                            = { il, _countof(il) };
    pd.PrimitiveTopologyType                  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets                       = 1;
    pd.RTVFormats[0]                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    pd.SampleDesc.Count                       = 1;
    pd.IBStripCutValue                        = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

    return SUCCEEDED(device_->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso_)));
}

// -- Helper: create a small upload-heap buffer with initial data ---------

static bool create_upload_buffer(ID3D12Device* dev,
                                 Microsoft::WRL::ComPtr<ID3D12Resource>& out,
                                 const void* data, UINT bytes) {
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width            = bytes;
    rd.Height           = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels        = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(dev->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&out))))
        return false;

    if (data) {
        void* mapped = nullptr;
        D3D12_RANGE rr = {};
        if (FAILED(out->Map(0, &rr, &mapped)))
            return false;
        std::memcpy(mapped, data, bytes);
        out->Unmap(0, nullptr);
    }
    return true;
}

// -- Init ---------------------------------------------------------------

bool Renderer::init(HWND hwnd, int32_t width, int32_t height) {
    width_  = width;
    height_ = height;
    stats_  = {};

    do {
        if (!create_device()) break;

        // Command queue.
        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(device_->CreateCommandQueue(&qd, IID_PPV_ARGS(&cmd_queue_))))
            break;

        // Swap chain (double-buffered, flip-discard).
        ComPtr<IDXGIFactory4> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            break;

        DXGI_SWAP_CHAIN_DESC1 scd = {};
        scd.Width       = static_cast<UINT>(width);
        scd.Height      = static_cast<UINT>(height);
        scd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.SampleDesc.Count = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount = k_frame_count;
        scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        ComPtr<IDXGISwapChain1> sc1;
        if (FAILED(factory->CreateSwapChainForHwnd(
                cmd_queue_.Get(), hwnd, &scd, nullptr, nullptr, &sc1)))
            break;
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(sc1.As(&swap_chain_)))
            break;

        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

        // RTV descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.NumDescriptors = k_frame_count;
        hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        if (FAILED(device_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtv_heap_))))
            break;

        rtv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        bool rtv_ok = true;
        for (uint32_t i = 0; i < k_frame_count; ++i) {
            if (FAILED(swap_chain_->GetBuffer(i, IID_PPV_ARGS(&render_targets_[i])))) {
                rtv_ok = false;
                break;
            }
            device_->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtv);
            rtv.ptr += rtv_size_;
        }
        if (!rtv_ok) break;

        // Command allocator + command list.
        if (FAILED(device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_alloc_))))
            break;
        if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                cmd_alloc_.Get(), nullptr, IID_PPV_ARGS(&cmd_list_))))
            break;
        cmd_list_->Close();

        // Fence.
        if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))))
            break;
        fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!fence_event_) break;

        // Pipeline (shaders + root sig + PSO).
        if (!create_pipeline()) break;

        // Static quad vertex buffer (4 vertices).
        if (!create_upload_buffer(device_.Get(), quad_vb_,
                k_quad_verts, sizeof(k_quad_verts)))
            break;

        // Static quad index buffer (6 indices).
        if (!create_upload_buffer(device_.Get(), quad_ib_,
                k_quad_indices, sizeof(k_quad_indices)))
            break;

        // Dynamic instance buffer (upload heap, persistently mapped).
        ib_capacity_ = k_max_render_agents;
        UINT ib_bytes = ib_capacity_ * static_cast<UINT>(sizeof(InstanceData));

        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width              = ib_bytes;
        rd.Height             = 1;
        rd.DepthOrArraySize   = 1;
        rd.MipLevels          = 1;
        rd.SampleDesc.Count   = 1;
        rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(device_->CreateCommittedResource(
                &hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&instance_buffer_))))
            break;

        D3D12_RANGE rr = {};
        if (FAILED(instance_buffer_->Map(0, &rr, &ib_mapped_)))
            break;

        return true;
    } while (false);

    shutdown();
    return false;
}

// -- Render (instanced) -------------------------------------------------

void Renderer::render(const RenderFrame& frame, const RenderCamera& camera) {
    wait_for_gpu();

    // Fill instance buffer.
    uint32_t instance_count = 0;
    {
        auto* inst = static_cast<InstanceData*>(ib_mapped_);
        uint32_t limit = (frame.extracted_count < ib_capacity_)
                         ? frame.extracted_count : ib_capacity_;

        for (uint32_t i = 0; i < limit; ++i) {
            const auto& a = frame.agents[i];
            uint32_t ci = (a.team_id < k_team_color_count) ? a.team_id : 0u;
            float hp = (a.health_pct > 0.0f) ? a.health_pct : 0.1f;

            inst[i].pos_x     = a.x;
            inst[i].pos_y     = a.y;
            inst[i].half_size = k_half_size;
            inst[i].r         = k_team_colors[ci][0] * hp;
            inst[i].g         = k_team_colors[ci][1] * hp;
            inst[i].b         = k_team_colors[ci][2] * hp;
            inst[i].a         = k_team_colors[ci][3];
        }
        instance_count = limit;
    }

    // Update stats.
    stats_.extracted_count = frame.extracted_count;
    stats_.instance_count  = instance_count;
    stats_.draw_call_count = (instance_count > 0) ? 1u : 0u;
    stats_.dropped_count   = (frame.extracted_count > ib_capacity_)
                             ? (frame.extracted_count - ib_capacity_) : 0u;

    cmd_alloc_->Reset();
    cmd_list_->Reset(cmd_alloc_.Get(), pso_.Get());

    // Build ortho matrix from camera.
    float ortho[16] = {};
    camera.build_ortho(ortho);

    cmd_list_->SetGraphicsRootSignature(root_sig_.Get());
    cmd_list_->SetGraphicsRoot32BitConstants(0, 16, ortho, 0);

    D3D12_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(width_);
    vp.Height   = static_cast<float>(height_);
    vp.MaxDepth = 1.0f;
    cmd_list_->RSSetViewports(1, &vp);

    D3D12_RECT sc = {};
    sc.right  = static_cast<LONG>(width_);
    sc.bottom = static_cast<LONG>(height_);
    cmd_list_->RSSetScissorRects(1, &sc);

    // Transition: PRESENT -> RENDER_TARGET.
    D3D12_RESOURCE_BARRIER bar = {};
    bar.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bar.Transition.pResource   = render_targets_[frame_index_].Get();
    bar.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    bar.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd_list_->ResourceBarrier(1, &bar);

    // Clear to dark background.
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(frame_index_) * rtv_size_;
    const float clear[] = { 0.08f, 0.08f, 0.12f, 1.0f };
    cmd_list_->ClearRenderTargetView(rtv, clear, 0, nullptr);
    cmd_list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    // Draw instanced crowd quads.
    if (instance_count > 0) {
        cmd_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Slot 0: quad vertices.
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = quad_vb_->GetGPUVirtualAddress();
        vbv.SizeInBytes    = sizeof(k_quad_verts);
        vbv.StrideInBytes  = sizeof(QuadVertex);

        // Slot 1: instance data.
        D3D12_VERTEX_BUFFER_VIEW ibv = {};
        ibv.BufferLocation = instance_buffer_->GetGPUVirtualAddress();
        ibv.SizeInBytes    = instance_count * static_cast<UINT>(sizeof(InstanceData));
        ibv.StrideInBytes  = static_cast<UINT>(sizeof(InstanceData));

        D3D12_VERTEX_BUFFER_VIEW views[] = { vbv, ibv };
        cmd_list_->IASetVertexBuffers(0, 2, views);

        // Index buffer.
        D3D12_INDEX_BUFFER_VIEW ixv = {};
        ixv.BufferLocation = quad_ib_->GetGPUVirtualAddress();
        ixv.SizeInBytes    = sizeof(k_quad_indices);
        ixv.Format         = DXGI_FORMAT_R16_UINT;
        cmd_list_->IASetIndexBuffer(&ixv);

        cmd_list_->DrawIndexedInstanced(6, instance_count, 0, 0, 0);
    }

    // Transition: RENDER_TARGET -> PRESENT.
    bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bar.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    cmd_list_->ResourceBarrier(1, &bar);

    cmd_list_->Close();

    ID3D12CommandList* lists[] = { cmd_list_.Get() };
    cmd_queue_->ExecuteCommandLists(1, lists);

    swap_chain_->Present(1, 0);

    ++fence_value_;
    cmd_queue_->Signal(fence_.Get(), fence_value_);
    if (fence_->GetCompletedValue() < fence_value_) {
        fence_->SetEventOnCompletion(fence_value_, fence_event_);
        WaitForSingleObject(fence_event_, INFINITE);
    }

    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
}

// -- Resize -------------------------------------------------------------

bool Renderer::resize(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) return false;
    if (width == width_ && height == height_) return true;

    wait_for_gpu();

    for (uint32_t i = 0; i < k_frame_count; ++i)
        render_targets_[i].Reset();

    HRESULT hr = swap_chain_->ResizeBuffers(
        k_frame_count, static_cast<UINT>(width), static_cast<UINT>(height),
        DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) return false;

    width_  = width;
    height_ = height;
    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < k_frame_count; ++i) {
        if (FAILED(swap_chain_->GetBuffer(i, IID_PPV_ARGS(&render_targets_[i]))))
            return false;
        device_->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtv);
        rtv.ptr += rtv_size_;
    }

    return true;
}

// -- Sync ---------------------------------------------------------------

void Renderer::wait_for_gpu() {
    if (!cmd_queue_ || !fence_) return;
    ++fence_value_;
    cmd_queue_->Signal(fence_.Get(), fence_value_);
    if (fence_->GetCompletedValue() < fence_value_) {
        fence_->SetEventOnCompletion(fence_value_, fence_event_);
        WaitForSingleObject(fence_event_, INFINITE);
    }
}

// -- Shutdown -----------------------------------------------------------

void Renderer::shutdown() {
    wait_for_gpu();

    if (ib_mapped_) {
        instance_buffer_->Unmap(0, nullptr);
        ib_mapped_ = nullptr;
    }
    if (fence_event_) {
        CloseHandle(fence_event_);
        fence_event_ = nullptr;
    }
}

}  // namespace de
