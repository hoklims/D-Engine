#include "Runtime/Engine.h"

namespace de {

bool Engine::init() {
    return init(EngineConfig{});
}

bool Engine::init(const EngineConfig& cfg) {
    config_ = cfg;

    WindowDesc desc;
    desc.title = "D-Engine 2.0";
    desc.width = 1280;
    desc.height = 720;

    if (!window_.create(desc)) return false;

    clock_.init();

    if (!fixed_step_.init(config_.sim_rate_hz, config_.max_frame_delta, config_.max_steps_per_frame)) {
        window_.destroy();
        return false;
    }

    running_ = true;
    return true;
}

void Engine::run() {
    while (running_) {
        begin_frame();
        if (!running_) break;
        tick_fixed_steps();
        render();
        end_frame();
    }
}

void Engine::shutdown() {
    running_ = false;
    window_.destroy();
}

const FrameInfo& Engine::frame_info() const {
    return frame_info_;
}

void Engine::begin_frame() {
    clock_.update();
    if (!window_.pump_messages()) {
        running_ = false;
    }

    ++frame_info_.frame_index;
    frame_info_.raw_frame_delta = clock_.delta_seconds();
}

void Engine::tick_fixed_steps() {
    FixedStepResult result = fixed_step_.consume(frame_info_.raw_frame_delta);

    frame_info_.clamped_frame_delta = result.clamped_delta;
    frame_info_.steps_this_frame = result.steps_taken;
    frame_info_.step_cap_hit = result.step_cap_hit;
    frame_info_.presentation_alpha = result.alpha;

    for (uint32_t i = 0; i < result.steps_taken; ++i) {
        ++frame_info_.sim_tick_index;
        update_fixed(fixed_step_.step_dt());
    }

    update_frame(result.alpha);
}

void Engine::update_frame(double alpha) {
    update_presentation(alpha);
}

void Engine::update_fixed(double step_dt) {
    // Future: ECS simulation tick
    // Future: crowd logic
    // Future: navigation / avoidance
    // Future: combat resolution
    (void)step_dt;
}

void Engine::update_presentation(double alpha) {
    // Future: interpolation for rendering
    // Future: animation blending
    (void)alpha;
}

void Engine::render() {
    // Future: DX12 frame submission
    // Future: crowd rendering pipeline
}

void Engine::end_frame() {
    // Future: frame telemetry
    // Future: replay capture
}

} // namespace de
