#pragma once

#include <cstdint>

namespace de {

// Maximum pixel-quad instances for the overlay pass.
static constexpr uint32_t k_max_overlay_instances = 4096;

// CPU-side instance data for overlay quads.
// Layout matches InstanceData in Renderer.cpp (pos, half_size, color, dir).
struct OverlayInstance {
    float pos_x, pos_y;
    float half_sx, half_sy;
    float r, g, b, a;
    float dir_x = 0.0f;
    float dir_y = 1.0f;   // no rotation
};

// Structured text lines extracted from runtime state.
struct DebugOverlayData {
    static constexpr int k_max_lines = 8;
    static constexpr int k_line_len  = 64;

    char lines[k_max_lines][k_line_len] = {};
    int  line_count = 0;

    void clear();
    void add(const char* text);
};

// Extract debug info from engine state into structured text lines.
// Counters must match the RenderStats contract:
//   agent_count   = total in world
//   visible_count = passed view culling
//   culled_count  = removed by view culling (extracted - visible)
//   dropped_count = not extracted (agent_count - extracted)
void extract_debug_overlay(
    const char* scene,
    bool        paused,
    uint64_t    tick,
    uint32_t    agent_count,
    uint32_t    visible_count,
    uint32_t    culled_count,
    uint32_t    dropped_count,
    bool        frame_skipped,
    bool        within_budget,
    DebugOverlayData& out);

// Generate pixel-quad instances for screen-space text rendering.
// Returns the number of instances written into out[].
uint32_t generate_overlay_instances(
    const DebugOverlayData& data,
    float screen_w, float screen_h,
    OverlayInstance* out, uint32_t max_count);

}  // namespace de
