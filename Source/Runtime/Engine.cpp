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

    frame_info_ = {};
    telemetry_ = {};
    wip_telemetry_ = {};
    sim_.bootstrap();
    running_ = true;
    return true;
}

void Engine::run() {
    while (running_) {
        wip_telemetry_ = {};

        {
            ScopeTimer total_timer(&wip_telemetry_.total_frame_s);

            begin_frame();
            if (!running_) break;
            tick_fixed_steps();
            render();
            end_frame();
        } // total_timer destroyed here -- total_frame_s now final

        telemetry_ = wip_telemetry_;
    }
}

void Engine::shutdown() {
    running_ = false;
    sim_.shutdown();
    window_.destroy();
}

const FrameInfo& Engine::frame_info() const {
    return frame_info_;
}

const FrameTelemetry& Engine::frame_telemetry() const {
    return telemetry_;
}

const SimState& Engine::sim_state() const {
    return sim_;
}

void Engine::begin_frame() {
    ScopeTimer t(&wip_telemetry_.begin_frame_s);
    clock_.update();
    if (!window_.pump_messages()) {
        running_ = false;
    }
}

void Engine::tick_fixed_steps() {
    ++frame_info_.frame_index;
    frame_info_.raw_frame_delta = clock_.delta_seconds();

    FixedStepResult result = fixed_step_.consume(frame_info_.raw_frame_delta);

    frame_info_.clamped_frame_delta = result.clamped_delta;
    frame_info_.steps_this_frame = result.steps_taken;
    frame_info_.step_cap_hit = result.step_cap_hit;
    frame_info_.presentation_alpha = result.alpha;

    for (uint32_t i = 0; i < result.steps_taken; ++i) {
        ++frame_info_.sim_tick_index;
        {
            ScopeTimer st(&wip_telemetry_.fixed_update_s, true);
            update_fixed(fixed_step_.step_dt());
        }
        ++wip_telemetry_.fixed_step_count;
    }

    {
        ScopeTimer st(&wip_telemetry_.presentation_update_s);
        update_frame(result.alpha);
    }
}

void Engine::update_frame(double alpha) {
    update_presentation(alpha);
}

void Engine::update_fixed(double step_dt) {
    sim_.tick(step_dt);
}

void Engine::update_presentation(double alpha) {
    // Future: interpolation for rendering
    // Future: animation blending
    (void)alpha;
}

void Engine::render() {
    ScopeTimer t(&wip_telemetry_.render_s);
    // Future: DX12 frame submission
    // Future: crowd rendering pipeline
}

void Engine::end_frame() {
    ScopeTimer t(&wip_telemetry_.end_frame_s);
    // Future: replay capture
}

} // namespace de
