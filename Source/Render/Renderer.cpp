#include "Render/Renderer.h"
#include "Render/RenderFrame.h"

#pragma warning(push, 3)
#include <d3dcompiler.h>
#pragma warning(pop)

#include <cstring>

namespace de {

// -- Inline HLSL --------------------------------------------------------

static const char k_shader_src[] = R"(
cbuffer OrthoMatrix : register(b0) {
    float4x4 ortho;
};

struct VSIn  { float2 pos : POSITION; float4 col : COLOR; };
struct PSIn  { float4 pos : SV_POSITION; float4 col : COLOR; };

PSIn VSMain(VSIn i) {
    PSIn o;
    o.pos = mul(ortho, float4(i.pos, 0.0, 1.0));
    o.col = i.col;
    return o;
}

float4 PSMain(PSIn i) : SV_TARGET { return i.col; }
)";

// -- Vertex layout ------------------------------------------------------

struct RenderVertex {
    float x, y;
    float r, g, b, a;
};

static constexpr uint32_t k_verts_per_agent = 6;
static constexpr float    k_half_size       = 0.3f;

static constexpr float k_team_colors[][4] = {
    { 0.9f, 0.2f, 0.2f, 1.0f },   // team 0: red
    { 0.2f, 0.4f, 0.9f, 1.0f },   // team 1: blue
    { 0.2f, 0.8f, 0.3f, 1.0f },   // team 2: green
    { 0.9f, 0.8f, 0.1f, 1.0f },   // team 3: yellow
};
static constexpr uint32_t k_team_color_count = 4;

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

    // Input layout.
    D3D12_INPUT_ELEMENT_DESC il[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,      0,  0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  8,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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

    return SUCCEEDED(device_->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso_)));
}

// -- Init ---------------------------------------------------------------

bool Renderer::init(HWND hwnd, int32_t width, int32_t height) {
    width_  = width;
    height_ = height;

    // Single-exit cleanup: any break triggers shutdown() which releases
    // every resource allocated so far (ComPtrs, HANDLE, mapped pointer).
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

        // Vertex buffer (upload heap, persistently mapped).
        vb_capacity_ = k_max_render_agents * k_verts_per_agent;
        UINT vb_bytes = vb_capacity_ * static_cast<UINT>(sizeof(RenderVertex));

        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width              = vb_bytes;
        rd.Height             = 1;
        rd.DepthOrArraySize   = 1;
        rd.MipLevels          = 1;
        rd.SampleDesc.Count   = 1;
        rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(device_->CreateCommittedResource(
                &hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&vertex_buffer_))))
            break;

        D3D12_RANGE rr = {};
        if (FAILED(vertex_buffer_->Map(0, &rr, &vb_mapped_)))
            break;

        return true;
    } while (false);

    // Partial init failed -- release everything already created.
    shutdown();
    return false;
}

// -- Render -------------------------------------------------------------

void Renderer::render(const RenderFrame& frame) {
    wait_for_gpu();

    cmd_alloc_->Reset();
    cmd_list_->Reset(cmd_alloc_.Get(), pso_.Get());

    // Fill vertex buffer with quads.
    uint32_t vert_count = 0;
    {
        auto* v = static_cast<RenderVertex*>(vb_mapped_);
        for (uint32_t i = 0; i < frame.extracted_count; ++i) {
            if (vert_count + k_verts_per_agent > vb_capacity_) break;

            const auto& a = frame.agents[i];
            float cx = a.x;
            float cy = a.y;
            float hs = k_half_size;

            uint32_t ci = (a.team_id < k_team_color_count) ? a.team_id : 0u;
            float hp = (a.health_pct > 0.0f) ? a.health_pct : 0.1f;
            float cr = k_team_colors[ci][0] * hp;
            float cg = k_team_colors[ci][1] * hp;
            float cb = k_team_colors[ci][2] * hp;
            float ca = k_team_colors[ci][3];

            // Quad: 2 triangles (TL, TR, BL) + (TR, BR, BL).
            v[vert_count++] = { cx - hs, cy + hs, cr, cg, cb, ca };
            v[vert_count++] = { cx + hs, cy + hs, cr, cg, cb, ca };
            v[vert_count++] = { cx - hs, cy - hs, cr, cg, cb, ca };
            v[vert_count++] = { cx + hs, cy + hs, cr, cg, cb, ca };
            v[vert_count++] = { cx + hs, cy - hs, cr, cg, cb, ca };
            v[vert_count++] = { cx - hs, cy - hs, cr, cg, cb, ca };
        }
    }

    // Orthographic projection: world [-hw, +hw] x [-hh, +hh] -> NDC [-1, 1].
    float hw = 50.0f;
    float hh = hw * static_cast<float>(height_) / static_cast<float>(width_);
    float ortho[16] = {};
    ortho[0]  = 1.0f / hw;    // row 0 col 0
    ortho[5]  = 1.0f / hh;    // row 1 col 1
    ortho[10] = 1.0f;          // row 2 col 2
    ortho[15] = 1.0f;          // row 3 col 3

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

    // Draw crowd quads.
    if (vert_count > 0) {
        cmd_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
        vbv.SizeInBytes    = vert_count * static_cast<UINT>(sizeof(RenderVertex));
        vbv.StrideInBytes  = static_cast<UINT>(sizeof(RenderVertex));
        cmd_list_->IASetVertexBuffers(0, 1, &vbv);
        cmd_list_->DrawInstanced(vert_count, 1, 0, 0);
    }

    // Transition: RENDER_TARGET -> PRESENT.
    bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bar.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    cmd_list_->ResourceBarrier(1, &bar);

    cmd_list_->Close();

    ID3D12CommandList* lists[] = { cmd_list_.Get() };
    cmd_queue_->ExecuteCommandLists(1, lists);

    swap_chain_->Present(1, 0);

    // Sync: wait for this frame to finish before reusing the allocator.
    ++fence_value_;
    cmd_queue_->Signal(fence_.Get(), fence_value_);
    if (fence_->GetCompletedValue() < fence_value_) {
        fence_->SetEventOnCompletion(fence_value_, fence_event_);
        WaitForSingleObject(fence_event_, INFINITE);
    }

    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
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

    if (vb_mapped_) {
        vertex_buffer_->Unmap(0, nullptr);
        vb_mapped_ = nullptr;
    }
    if (fence_event_) {
        CloseHandle(fence_event_);
        fence_event_ = nullptr;
    }
}

}  // namespace de
