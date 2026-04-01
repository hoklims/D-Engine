#include "Runtime/Engine.h"
#include "Runtime/DemoPresets.h"
#include "Render/Renderer.h"

#include <cstdio>

namespace de {

bool Engine::init() {
    EngineConfig cfg;
    cfg.demo_preset = 0;   // DEngine.exe starts on first preset (LaneClash).
    return init(cfg);
}

bool Engine::init(const EngineConfig& cfg) {
    config_ = cfg;

    // Reset all public state up front so that no stale data survives
    // even if init() fails early and the caller inspects accessors.
    frame_info_ = {};
    telemetry_ = {};
    wip_telemetry_ = {};
    render_frame_ = {};
    culled_frame_ = {};
    render_stats_ = {};
    overlay_data_ = {};
    hud_data_ = {};
    hud_mode_ = HudMode::Full;
    overlay_count_ = 0;
    debug_ = {};
    current_preset_ = -1;
    world_debug_config_ = {};

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

    if (config_.demo_preset >= 0 && config_.demo_preset < k_demo_preset_count) {
        current_preset_ = config_.demo_preset;
        const auto& p = k_demo_presets[current_preset_];
        apply_demo_preset(sim_, p);
        world_debug_config_ = p.world_debug;
        debug_.manual_hw = p.camera_hw;
    } else {
        switch (config_.start_scene) {
        case StartScene::Battlefield: sim_.bootstrap_battlefield({}); break;
        case StartScene::Basic:       sim_.bootstrap();               break;
        case StartScene::Crowd:       // fall-through
        default:                      sim_.bootstrap_crowd();         break;
        }
    }

    generate_world_debug(world_debug_config_, world_debug_data_);

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
        process_debug_input();
        apply_debug_actions();
        if (debug_.step_requested) {
            // Single-step: exactly 1 sim tick, bypass fixed-step accumulator.
            tick_single_step();
        } else if (debug_.should_tick()) {
            tick_fixed_steps();
        } else {
            // Still extract render frame for display even when paused.
            update_presentation(0.0);
        }
        render();
        end_frame();
        update_window_title();
        debug_.consume();
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
    overlay_data_ = {};
    hud_data_ = {};
    hud_mode_ = HudMode::Full;
    overlay_count_ = 0;
    debug_ = {};
    current_preset_ = -1;
    world_debug_config_ = {};
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

const DebugOverlayData& Engine::debug_overlay() const {
    return overlay_data_;
}

const DebugHudData& Engine::debug_hud() const {
    return hud_data_;
}

HudMode Engine::hud_mode() const {
    return hud_mode_;
}

const DebugControls& Engine::debug_controls() const {
    return debug_;
}

DebugControls& Engine::debug_controls_mut() {
    return debug_;
}

const FixedStep& Engine::fixed_step() const {
    return fixed_step_;
}

int8_t Engine::current_preset() const {
    return current_preset_;
}

const char* Engine::current_scene_label() const {
    const char* pn = preset_name(current_preset_);
    return pn ? pn : scene_name(config_.start_scene);
}

const WorldDebugConfig& Engine::world_debug_config() const {
    return world_debug_config_;
}

// -- Debug controls -------------------------------------------------

void Engine::process_debug_input() {
    DebugAction action = {};

    // One-shot actions: initial key press only (no auto-repeat).
    for (uint32_t i = 0; i < window_.pressed_count(); ++i) {
        uint8_t vk = window_.pressed_at(i);
        switch (vk) {
        case 0x20: action.toggle_pause      = true; break; // VK_SPACE
        case 'N':  action.single_step       = true; break;
        case 'R':  action.reset_scene       = true; break;
        case '1':  action.switch_preset     = 0;    break; // LaneClash
        case '2':  action.switch_preset     = 1;    break; // DenseMelee
        case '3':  action.switch_preset     = 2;    break; // WallGap
        case '4':  action.switch_preset     = 3;    break; // SparseApproach
        case 'F':  action.toggle_auto_frame = true;  break;
        case 'H':  hud_mode_ = toggle_hud_visibility(hud_mode_); break;
        case 0x09: // VK_TAB
            if (hud_mode_ != HudMode::Hidden)
                hud_mode_ = next_hud_mode(hud_mode_);
            break;
        default: break;
        }
    }

    // Continuous actions: include auto-repeat (held keys).
    for (uint32_t i = 0; i < window_.repeat_count(); ++i) {
        uint8_t vk = window_.repeat_at(i);
        switch (vk) {
        case 0xBB: action.zoom_delta += 5.0f;  break; // VK_OEM_PLUS
        case 0xBD: action.zoom_delta -= 5.0f;  break; // VK_OEM_MINUS
        case VK_LEFT:  action.pan_dx -= 3.0f;  break;
        case VK_RIGHT: action.pan_dx += 3.0f;  break;
        case VK_UP:    action.pan_dy += 3.0f;  break;
        case VK_DOWN:  action.pan_dy -= 3.0f;  break;
        default: break;
        }
    }

    window_.clear_keys();
    debug_.apply(action);
}

void Engine::apply_debug_actions() {
    // Scene reset.
    if (debug_.reset_requested) {
        sim_.shutdown();
        if (current_preset_ >= 0 && current_preset_ < k_demo_preset_count) {
            apply_demo_preset(sim_, k_demo_presets[current_preset_]);
        } else {
            switch (config_.start_scene) {
            case StartScene::Battlefield: sim_.bootstrap_battlefield({}); break;
            case StartScene::Basic:       sim_.bootstrap();               break;
            case StartScene::Crowd:
            default:                      sim_.bootstrap_crowd();         break;
            }
        }
        frame_info_ = {};
        fixed_step_.reset();
    }

    // Preset switch (keyboard path, takes priority over scene switch).
    if (debug_.preset_switch >= 0 && debug_.preset_switch < k_demo_preset_count) {
        current_preset_ = debug_.preset_switch;
        const auto& p = k_demo_presets[current_preset_];
        sim_.shutdown();
        apply_demo_preset(sim_, p);
        world_debug_config_ = p.world_debug;
        generate_world_debug(world_debug_config_, world_debug_data_);
        debug_.manual_hw = p.camera_hw;
        frame_info_ = {};
        fixed_step_.reset();
    }
    // Scene switch (test/legacy path).
    else if (debug_.scene_switch >= 0) {
        current_preset_ = -1;
        auto scene = static_cast<StartScene>(debug_.scene_switch);
        config_.start_scene = scene;
        sim_.shutdown();
        switch (scene) {
        case StartScene::Battlefield: sim_.bootstrap_battlefield({}); break;
        case StartScene::Basic:       sim_.bootstrap();               break;
        case StartScene::Crowd:
        default:                      sim_.bootstrap_crowd();         break;
        }
        world_debug_config_ = {};
        generate_world_debug(world_debug_config_, world_debug_data_);
        frame_info_ = {};
        fixed_step_.reset();
    }
}

void Engine::update_window_title() {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "D-Engine 2.0 | %s | %s | tick %llu | agents %u | vis %u | cull %u",
        current_scene_label(),
        debug_.paused ? "PAUSED" : "RUNNING",
        static_cast<unsigned long long>(frame_info_.sim_tick_index),
        render_stats_.agent_count,
        render_stats_.visible_count,
        render_stats_.culled_count);
    window_.set_title(buf);
}

// -----------------------------------------------------------------------

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

void Engine::tick_single_step() {
    ++frame_info_.frame_index;
    frame_info_.raw_frame_delta = 0.0;
    frame_info_.clamped_frame_delta = 0.0;
    frame_info_.steps_this_frame = 1;
    frame_info_.step_cap_hit = false;
    frame_info_.presentation_alpha = 0.0;

    ++frame_info_.sim_tick_index;
    {
        ScopeTimer st(&wip_telemetry_.fixed_update_s, true);
        update_fixed(fixed_step_.step_dt());
    }
    ++wip_telemetry_.fixed_step_count;

    {
        ScopeTimer st(&wip_telemetry_.presentation_update_s);
        update_frame(0.0);
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

    if (debug_.auto_frame) {
        camera_ = auto_frame_crowd(render_frame_, aspect);
        // Sync manual camera to auto-frame so toggle is seamless.
        debug_.manual_center_x = camera_.center_x;
        debug_.manual_center_y = camera_.center_y;
        debug_.manual_hw       = camera_.half_width;
    } else {
        camera_.center_x  = debug_.manual_center_x;
        camera_.center_y  = debug_.manual_center_y;
        camera_.half_width = debug_.manual_hw;
        camera_.aspect    = aspect;
    }

    // View culling: work on a copy so render_frame_ stays a pristine
    // extraction snapshot accessible via render_frame().
    culled_frame_ = render_frame_;
    uint32_t visible = cull_render_frame(culled_frame_, camera_);
    culled_frame_.extracted_count = visible;
}

void Engine::render() {
    ScopeTimer t(&wip_telemetry_.render_s);

    // Derive culling counters from the pristine frame vs culled work buffer.
    uint32_t extracted = render_frame_.extracted_count;
    uint32_t visible   = culled_frame_.extracted_count;
    uint32_t culled    = extracted - visible;
    uint32_t drop_cap  = render_frame_.agent_count - extracted;

    // Structured HUD extraction + instance generation.
    extract_debug_hud(
        hud_mode_,
        current_scene_label(),
        debug_.paused,
        frame_info_.sim_tick_index,
        debug_.auto_frame,
        render_frame_.agent_count,
        render_frame_.extracted_count,
        visible,
        culled,
        drop_cap,
        !renderer_active_,
        sim_.budget_status().within_budget,
        telemetry_.total_frame_s * 1000.0,
        telemetry_.fixed_update_s * 1000.0,
        hud_data_);
    overlay_count_ = generate_hud_instances(
        hud_data_,
        static_cast<float>(window_.width()),
        static_cast<float>(window_.height()),
        overlay_instances_, k_max_overlay_instances);

    // Backward compat: keep flat overlay populated for test accessor.
    extract_debug_overlay(
        current_scene_label(),
        debug_.paused,
        frame_info_.sim_tick_index,
        render_frame_.agent_count,
        visible,
        culled,
        drop_cap,
        !renderer_active_,
        sim_.budget_status().within_budget,
        overlay_data_);

    if (renderer_active_) {
        if (!renderer_->resize(window_.width(), window_.height())) {
            renderer_active_ = false;
            render_stats_ = {};
            render_stats_.agent_count     = render_frame_.agent_count;
            render_stats_.extracted_count = extracted;
            render_stats_.visible_count   = visible;
            render_stats_.culled_count    = culled;
            render_stats_.dropped_count   = render_frame_.agent_count;
            render_stats_.frame_skipped   = true;
            return;
        }
        // Submit culled work buffer (only visible agents) to the renderer.
        renderer_->render(culled_frame_, camera_, &world_debug_data_,
                         overlay_instances_, overlay_count_);
        render_stats_ = renderer_->stats();
    } else {
        // Headless: no GPU submission.
        render_stats_ = {};
        render_stats_.agent_count    = render_frame_.agent_count;
        render_stats_.instance_count = 0;
        render_stats_.dropped_count  = render_frame_.agent_count;
    }

    // Fix up culling stats (renderer only sees the culled frame).
    render_stats_.extracted_count = extracted;
    render_stats_.visible_count   = visible;
    render_stats_.culled_count    = culled;
}

void Engine::end_frame() {
    ScopeTimer t(&wip_telemetry_.end_frame_s);
    // Future: replay capture
}

} // namespace de
