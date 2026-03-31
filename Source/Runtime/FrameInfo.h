#pragma once

#include <cstdint>

namespace de {

// Read-only snapshot of the runtime timeline for the current frame.
// Maintained by the engine loop. Consumed by future subsystems
// (telemetry, replay hash, budget overlay, debug HUD).
struct FrameInfo {
    uint64_t frame_index = 0;
    uint64_t sim_tick_index = 0;

    uint32_t steps_this_frame = 0;
    bool step_cap_hit = false;

    double raw_frame_delta = 0.0;
    double clamped_frame_delta = 0.0;
    double presentation_alpha = 0.0;
};

} // namespace de
