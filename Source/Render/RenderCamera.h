#pragma once

#include "Render/RenderFrame.h"

namespace de {

// Orthographic camera for crowd rendering.
// Stores center, half-width, and aspect ratio.
// Produces a column-major 4x4 ortho matrix for DX12 root constants.
struct RenderCamera {
    float center_x    = 0.0f;
    float center_y    = 0.0f;
    float half_width  = 50.0f;   // world units visible from center to edge (x)
    float aspect      = 16.0f / 9.0f;

    // Derived: half_height = half_width / aspect.
    float half_height() const;

    // Write a 4x4 ortho matrix into out[16] in HLSL column-major order.
    // Column-major layout: out[0..3] = col0, out[4..7] = col1, etc.
    // Consumed by HLSL mul(ortho, float4(pos,0,1)) via root constants.
    // Maps [center-hw, center+hw] x [center-hh, center+hh] to NDC [-1,1].
    void build_ortho(float out[16]) const;
};

// Padding factor around the crowd bounding box (fraction of extent).
static constexpr float k_auto_frame_margin = 0.15f;

// Minimum half-width to avoid degenerate zoom on tiny / single-agent scenes.
static constexpr float k_min_half_width = 5.0f;

// Compute a RenderCamera that frames all agents in the RenderFrame.
// Falls back to a default camera if the frame is empty.
RenderCamera auto_frame_crowd(const RenderFrame& frame, float aspect);

}  // namespace de
