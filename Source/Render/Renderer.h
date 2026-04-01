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

#include "Render/RenderStats.h"

namespace de {

struct RenderFrame;
struct RenderCamera;
struct WorldDebugData;
struct OverlayInstance;

// Minimal DX12 instanced renderer -- draws crowd as colored quads.
// One shared quad (4 verts + 6 indices), one instance per agent.
// Synchronous (CPU waits for GPU each frame). No depth buffer, no MSAA.
struct Renderer {
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(HWND hwnd, int32_t width, int32_t height);
    void render(const RenderFrame& frame, const RenderCamera& camera,
                const WorldDebugData* world_debug = nullptr,
                const OverlayInstance* overlay = nullptr,
                uint32_t overlay_count = 0);
    bool resize(int32_t width, int32_t height);
    void shutdown();

    const RenderStats& stats() const { return stats_; }

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

    // Static quad geometry (4 verts + 6 indices).
    ComPtr<ID3D12Resource>            quad_vb_;
    ComPtr<ID3D12Resource>            quad_ib_;

    // Dynamic instance buffer (upload heap, persistently mapped).
    ComPtr<ID3D12Resource>            instance_buffer_;
    void*                             ib_mapped_     = nullptr;
    uint32_t                          ib_capacity_   = 0;

    int32_t width_  = 0;
    int32_t height_ = 0;

    RenderStats stats_;

    void wait_for_gpu();
    bool create_device();
    bool create_pipeline();
};

}  // namespace de
