#include "Runtime/FixedStep.h"

#include <algorithm>

namespace de {

bool FixedStep::init(double sim_rate_hz, double max_frame_delta, uint32_t max_steps_per_frame) {
    if (sim_rate_hz <= 0.0) return false;
    if (max_frame_delta < 0.0) return false;
    if (max_steps_per_frame == 0) return false;

    step_dt_ = 1.0 / sim_rate_hz;
    max_frame_delta_ = max_frame_delta;
    max_steps_ = max_steps_per_frame;
    accumulator_ = 0.0;
    return true;
}

FixedStepResult FixedStep::consume(double frame_delta) {
    double clamped = std::clamp(frame_delta, 0.0, max_frame_delta_);
    accumulator_ += clamped;

    uint32_t steps = 0;
    while (accumulator_ >= step_dt_ && steps < max_steps_) {
        accumulator_ -= step_dt_;
        ++steps;
    }

    bool cap_hit = (steps == max_steps_ && accumulator_ >= step_dt_);

    // If we hit the cap, drain leftover to prevent unbounded growth.
    if (cap_hit) {
        accumulator_ = 0.0;
    }

    double alpha = accumulator_ / step_dt_;
    return {steps, alpha, clamped, cap_hit};
}

void FixedStep::reset() { accumulator_ = 0.0; }

double FixedStep::step_dt() const { return step_dt_; }
double FixedStep::accumulator() const { return accumulator_; }

} // namespace de
