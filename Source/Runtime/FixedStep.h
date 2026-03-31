#pragma once

#include <cstdint>

namespace de {

struct FixedStepResult {
    uint32_t steps_taken;
    double alpha;
};

struct FixedStep {
    // Returns false if any parameter is invalid:
    //   sim_rate_hz must be > 0
    //   max_frame_delta must be >= 0
    //   max_steps_per_frame must be > 0
    bool init(double sim_rate_hz, double max_frame_delta, uint32_t max_steps_per_frame);

    // Feed a raw frame delta and compute how many fixed steps to run.
    FixedStepResult consume(double frame_delta);

    double step_dt() const;
    double accumulator() const;

private:
    double step_dt_ = 1.0 / 60.0;
    double max_frame_delta_ = 0.25;
    uint32_t max_steps_ = 8;
    double accumulator_ = 0.0;
};

} // namespace de
