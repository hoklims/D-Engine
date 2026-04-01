#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#pragma warning(push, 3)
#include <d3d12.h>
#include <dxgi1_4.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4265)
#include <wrl/client.h>
#pragma warning(pop)

#include <cstdint>

namespace de {

struct RenderFrame;

// Minimal DX12 renderer -- clears screen and draws crowd as colored quads.
// Synchronous (CPU waits for GPU each frame). No depth buffer, no MSAA.
struct Renderer {
    bool init(HWND hwnd, int32_t width, int32_t height);
    void render(const RenderFrame& frame);
    void shutdown();

private:
    static constexpr uint32_t k_frame_count = 2;

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3D12Device>              device_;
    ComPtr<ID3D12CommandQueue>        cmd_queue_;
    ComPtr<IDXGISwapChain3>           swap_chain_;
    ComPtr<ID3D12DescriptorHeap>      rtv_heap_;
    ComPtr<ID3D12Resource>            render_targets_[k_frame_count];
    ComPtr<ID3D12CommandAllocator>    cmd_alloc_;
    ComPtr<ID3D12GraphicsCommandList> cmd_list_;
    ComPtr<ID3D12Fence>               fence_;
    HANDLE                            fence_event_ = nullptr;
    uint64_t                          fence_value_ = 0;
    uint32_t                          frame_index_ = 0;
    uint32_t                          rtv_size_    = 0;

    ComPtr<ID3D12RootSignature>       root_sig_;
    ComPtr<ID3D12PipelineState>       pso_;
    ComPtr<ID3D12Resource>            vertex_buffer_;
    void*                             vb_mapped_   = nullptr;
    uint32_t                          vb_capacity_ = 0;

    int32_t width_  = 0;
    int32_t height_ = 0;

    void wait_for_gpu();
    bool create_device();
    bool create_pipeline();
};

}  // namespace de
