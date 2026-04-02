#pragma once

#include <cstdint>

namespace de {

struct World;

// Per-agent data extracted from the ECS for rendering.
// Read-only snapshot -- no pointers back to the World.
// Direction contract:
//   1. Velocity (if |v| > epsilon)  -- actual movement
//   2. DesiredDirection (if |dd| > epsilon) -- intent at rest
//   3. (0,1) -- stable default
struct CrowdRenderItem {
    float    x          = 0.0f;
    float    y          = 0.0f;
    uint8_t  team_id    = 0;
    uint8_t  lod_tier   = 0;
    float    health_pct = 1.0f;   // 0.0 - 1.0
    float    dir_x      = 0.0f;   // normalized facing direction
    float    dir_y      = 1.0f;   // (0,1) = default up
    bool     has_target = false;  // alive enemy target (not self, different team)
};

static constexpr uint32_t k_max_render_agents = 4096;

// Snapshot of crowd state for the renderer.
// Extracted once per frame from the committed simulation state.
struct RenderFrame {
    uint64_t sim_tick        = 0;
    uint64_t frame_id        = 0;
    uint32_t agent_count     = 0;   // total crowd agents in world
    uint32_t extracted_count = 0;   // actually extracted (capped at k_max_render_agents)
    CrowdRenderItem agents[k_max_render_agents] = {};

    // Bounding box of ALL crowd agents (not just extracted).
    // Used by auto-frame camera to reflect the true extent of the crowd
    // even when agent_count > k_max_render_agents.
    float crowd_min_x = 0.0f;
    float crowd_max_x = 0.0f;
    float crowd_min_y = 0.0f;
    float crowd_max_y = 0.0f;
    bool  has_crowd_bounds = false;
};

// Extract crowd agents from the World into a RenderFrame.
// Returns the number of agents extracted (capped at k_max_render_agents).
uint32_t extract_render_frame(World& world, uint64_t tick, uint64_t frame_id, RenderFrame& out);

}  // namespace de
