#include "Render/RenderCamera.h"

#include <algorithm>
#include <cstring>

namespace de {

float RenderCamera::half_height() const {
    return (aspect > 0.0f) ? (half_width / aspect) : half_width;
}

void RenderCamera::build_ortho(float out[16]) const {
    std::memset(out, 0, 16 * sizeof(float));

    float hw = half_width;
    float hh = half_height();

    // Avoid division by zero.
    if (hw <= 0.0f) hw = k_min_half_width;
    if (hh <= 0.0f) hh = k_min_half_width;

    // HLSL column-major layout: out[0..3]=col0, out[4..7]=col1, etc.
    // HLSL sees the matrix as:
    //   [out[0]  out[4]  out[8]   out[12]]
    //   [out[1]  out[5]  out[9]   out[13]]
    //   [out[2]  out[6]  out[10]  out[14]]
    //   [out[3]  out[7]  out[11]  out[15]]
    //
    // We want:
    //   [1/hw   0    0   -cx/hw ]
    //   [ 0   1/hh  0   -cy/hh ]
    //   [ 0    0    1      0   ]
    //   [ 0    0    0      1   ]
    out[0]  = 1.0f / hw;           // col0 row0
    out[5]  = 1.0f / hh;           // col1 row1
    out[10] = 1.0f;                // col2 row2
    out[12] = -center_x / hw;      // col3 row0
    out[13] = -center_y / hh;      // col3 row1
    out[15] = 1.0f;                // col3 row3
}

RenderCamera auto_frame_crowd(const RenderFrame& frame, float aspect) {
    RenderCamera cam;
    cam.aspect = (aspect > 0.0f) ? aspect : 1.0f;

    if (frame.extracted_count == 0) {
        cam.center_x   = 0.0f;
        cam.center_y   = 0.0f;
        cam.half_width = k_min_half_width;
        return cam;
    }

    // Compute bounding box of all extracted agents.
    float min_x = frame.agents[0].x;
    float max_x = frame.agents[0].x;
    float min_y = frame.agents[0].y;
    float max_y = frame.agents[0].y;

    for (uint32_t i = 1; i < frame.extracted_count; ++i) {
        float ax = frame.agents[i].x;
        float ay = frame.agents[i].y;
        if (ax < min_x) min_x = ax;
        if (ax > max_x) max_x = ax;
        if (ay < min_y) min_y = ay;
        if (ay > max_y) max_y = ay;
    }

    cam.center_x = (min_x + max_x) * 0.5f;
    cam.center_y = (min_y + max_y) * 0.5f;

    float extent_x = (max_x - min_x) * 0.5f;
    float extent_y = (max_y - min_y) * 0.5f;

    // Apply margin.
    extent_x *= (1.0f + k_auto_frame_margin);
    extent_y *= (1.0f + k_auto_frame_margin);

    // Pick half_width so that both x and y extents fit.
    // half_width >= extent_x  AND  half_width / aspect >= extent_y
    float hw_from_x = extent_x;
    float hw_from_y = extent_y * cam.aspect;

    cam.half_width = std::max({hw_from_x, hw_from_y, k_min_half_width});

    return cam;
}

}  // namespace de
