#include "Runtime/Engine.h"
#include "Render/Renderer.h"

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
    render_frame_ = {};
    render_stats_ = {};

    switch (config_.start_scene) {
    case StartScene::Battlefield: sim_.bootstrap_battlefield({}); break;
    case StartScene::Basic:       sim_.bootstrap();               break;
    case StartScene::Crowd:       // fall-through
    default:                      sim_.bootstrap_crowd();         break;
    }

    if (config_.enable_renderer) {
        renderer_ = new Renderer();
        renderer_active_ = renderer_->init(window_.handle(), window_.width(), window_.height());
        if (!renderer_active_) {
            delete renderer_;
            renderer_ = nullptr;
        }
    }

    running_ = true;
    return true;
}

void Engine::run() {
    while (running_) {
        step_one_frame();
    }
}

void Engine::step_one_frame() {
    if (!running_) return;
    wip_telemetry_ = {};
    {
        ScopeTimer total_timer(&wip_telemetry_.total_frame_s);
        begin_frame();
        if (!running_) return;
        tick_fixed_steps();
        render();
        end_frame();
    }
    telemetry_ = wip_telemetry_;
}

void Engine::shutdown() {
    running_ = false;
    if (renderer_) {
        renderer_->shutdown();
        delete renderer_;
        renderer_ = nullptr;
        renderer_active_ = false;
    }
    sim_.shutdown();
    window_.destroy();
    render_stats_ = {};
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

const RenderFrame& Engine::render_frame() const {
    return render_frame_;
}

const RenderCamera& Engine::render_camera() const {
    return camera_;
}

const RenderStats& Engine::render_stats() const {
    return render_stats_;
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
    (void)alpha;
    extract_render_frame(sim_.world, frame_info_.sim_tick_index,
                         frame_info_.frame_index, render_frame_);

    float w = static_cast<float>(window_.width());
    float h = static_cast<float>(window_.height());
    float aspect = (h > 0.0f) ? (w / h) : 1.0f;
    camera_ = auto_frame_crowd(render_frame_, aspect);
}

void Engine::render() {
    ScopeTimer t(&wip_telemetry_.render_s);
    if (renderer_active_) {
        if (!renderer_->resize(window_.width(), window_.height())) {
            renderer_active_ = false;
            // Publish coherent stats for the skipped frame.
            render_stats_ = {};
            render_stats_.agent_count     = render_frame_.agent_count;
            render_stats_.extracted_count = render_frame_.extracted_count;
            render_stats_.dropped_count   = render_frame_.agent_count;
            render_stats_.frame_skipped   = true;
            return;
        }
        renderer_->render(render_frame_, camera_);
        render_stats_ = renderer_->stats();
    } else {
        // Headless: populate stats from RenderFrame for telemetry consistency.
        render_stats_ = {};
        render_stats_.agent_count     = render_frame_.agent_count;
        render_stats_.extracted_count = render_frame_.extracted_count;
        render_stats_.dropped_count   = render_frame_.agent_count;
    }
}

void Engine::end_frame() {
    ScopeTimer t(&wip_telemetry_.end_frame_s);
    // Future: replay capture
}

} // namespace de
