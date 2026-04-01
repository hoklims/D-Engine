#pragma once

#include <cstdint>

namespace de {

// Configuration for the world debug overlay (ground, grid, axes).
struct WorldDebugConfig {
    bool  show_ground  = true;
    bool  show_grid    = true;
    float world_extent = 100.0f;   // half-size of ground plane
    float grid_spacing = 10.0f;    // distance between grid lines
};

static constexpr uint32_t k_max_world_debug_instances = 512;

// CPU-side instance data for debug geometry.
// Layout matches the renderer InstanceData (pos, half_size xy, color).
struct WorldDebugData {
    struct Instance {
        float pos_x, pos_y;
        float half_sx, half_sy;
        float r, g, b, a;
    };
    Instance items[k_max_world_debug_instances];
    uint32_t count = 0;
};

// Generate debug geometry from config. Idempotent, no allocations.
void generate_world_debug(const WorldDebugConfig& config, WorldDebugData& out);

} // namespace de
