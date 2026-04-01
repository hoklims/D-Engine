#pragma once

#include "Render/RenderCamera.h"
#include "Render/RenderFrame.h"

namespace de {

// Axis-aligned view bounds for CPU culling.
struct ViewBounds {
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
};

// Margin around the view for agent extent (world units).
// Slightly larger than k_half_size (0.3) to avoid popping at edges.
static constexpr float k_cull_margin = 0.5f;

// Compute AABB view bounds from camera, enlarged by margin.
ViewBounds compute_view_bounds(const RenderCamera& cam,
                               float margin = k_cull_margin);

// Point-in-AABB test.
inline bool is_in_view(float x, float y, const ViewBounds& b) {
    return x >= b.min_x && x <= b.max_x && y >= b.min_y && y <= b.max_y;
}

// Cull agents outside the camera view.
// Compacts visible agents to the front of frame.agents[].
// Does NOT modify frame.agent_count or frame.extracted_count.
// Returns the number of visible agents.
uint32_t cull_render_frame(RenderFrame& frame, const RenderCamera& cam);

}  // namespace de
