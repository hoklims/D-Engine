#pragma once

#include <cstdint>

namespace de {

// Per-frame render telemetry snapshot.
//
// Contrat:
//   agent_count              = total crowd agents in the World (before any cap)
//   extracted_count          = agents extracted into RenderFrame (capped at k_max_render_agents)
//   visible_count            = agents that passed view culling
//   culled_count             = agents removed by view culling (extracted - visible)
//   instance_count           = crowd instances submitted to the GPU draw call
//   dropped_count            = agent_count - instance_count  (agents NOT rendered)
//   draw_call_count          = total DrawIndexedInstanced calls (world + crowd + overlay)
//   world_draw_call_count    = draw calls for world debug pass only (0 or 1)
//   overlay_draw_call_count  = draw calls for debug overlay pass only (0 or 1)
//   frame_skipped            = true if the frame was not rendered (resize failure etc.)
//
// Invariants:
//   visible_count + culled_count == extracted_count
//   instance_count + dropped_count == agent_count
//   instance_count <= visible_count
//   draw_call_count == world_draw_call_count + overlay_draw_call_count
//                    + (instance_count > 0 ? 1 : 0)
struct RenderStats {
    uint32_t agent_count             = 0;
    uint32_t extracted_count         = 0;
    uint32_t visible_count           = 0;
    uint32_t culled_count            = 0;
    uint32_t instance_count          = 0;
    uint32_t draw_call_count         = 0;
    uint32_t dropped_count           = 0;
    uint32_t world_draw_call_count   = 0;
    uint32_t overlay_draw_call_count = 0;
    bool     frame_skipped           = false;
};

}  // namespace de
