#include "Render/ViewCulling.h"

namespace de {

ViewBounds compute_view_bounds(const RenderCamera& cam, float margin) {
    float hh = cam.half_height();
    return {
        cam.center_x - cam.half_width - margin,
        cam.center_x + cam.half_width + margin,
        cam.center_y - hh - margin,
        cam.center_y + hh + margin,
    };
}

uint32_t cull_render_frame(RenderFrame& frame, const RenderCamera& cam) {
    ViewBounds bounds = compute_view_bounds(cam);
    uint32_t write = 0;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        if (is_in_view(frame.agents[i].x, frame.agents[i].y, bounds)) {
            if (write != i) {
                frame.agents[write] = frame.agents[i];
            }
            ++write;
        }
    }
    return write;
}

}  // namespace de
