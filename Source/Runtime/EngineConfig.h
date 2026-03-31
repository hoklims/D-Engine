#pragma once

#include <cstdint>

namespace de {

struct EngineConfig {
    // Fixed simulation rate in Hz.
    double sim_rate_hz = 60.0;

    // Maximum wall-clock delta allowed per frame (seconds).
    // Larger spikes are clamped to this value.
    double max_frame_delta = 0.25;

    // Maximum number of fixed steps consumed per frame.
    // Prevents spiral-of-death when the sim can't keep up.
    uint32_t max_steps_per_frame = 8;
};

} // namespace de
