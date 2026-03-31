#pragma once

#include <cstdint>

namespace de {

// Per-frame CPU cost snapshot (seconds).
// Maintained by the engine loop alongside FrameInfo.
// Consumed by future subsystems (budget contracts, throttle LOD, overlay, replay).
struct FrameTelemetry {
    double begin_frame_s = 0.0;
    double fixed_update_s = 0.0;          // accumulated across all fixed steps
    double presentation_update_s = 0.0;
    double render_s = 0.0;
    double end_frame_s = 0.0;
    double total_frame_s = 0.0;

    uint32_t fixed_step_count = 0;
};

} // namespace de
