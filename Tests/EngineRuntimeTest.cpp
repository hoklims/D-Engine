#include "Runtime/Engine.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("FAIL: %s\n", name);
    }
}

// =================================================================
//  Crowd scene (default) -- headless init + step produces agents
// =================================================================

static void test_crowd_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "crowd headless: init ok");

    engine.step_one_frame();

    const auto& rf = engine.render_frame();
    check(rf.extracted_count > 0,
          "crowd headless: agents extracted after step");
    check(rf.extracted_count == rf.agent_count,
          "crowd headless: all agents extracted");
    check(rf.frame_id == 1,
          "crowd headless: frame_id == 1");

    engine.shutdown();
}

// =================================================================
//  Basic scene -- no crowd agents in render frame
// =================================================================

static void test_basic_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Basic;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "basic headless: init ok");

    engine.step_one_frame();

    const auto& rf = engine.render_frame();
    check(rf.extracted_count == 0,
          "basic headless: no crowd agents");

    engine.shutdown();
}

// =================================================================
//  Battlefield scene -- crowd agents present
// =================================================================

static void test_battlefield_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Battlefield;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "battlefield headless: init ok");

    engine.step_one_frame();

    const auto& rf = engine.render_frame();
    check(rf.extracted_count > 0,
          "battlefield headless: agents extracted");

    engine.shutdown();
}

// =================================================================
//  Multiple frames -- render frame stays coherent
// =================================================================

static void test_multi_frame() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "multi frame: init ok");

    for (int i = 0; i < 5; ++i)
        engine.step_one_frame();

    const auto& rf = engine.render_frame();
    check(rf.extracted_count > 0,
          "multi frame: agents after 5 frames");
    check(rf.frame_id == 5,
          "multi frame: frame_id == 5");

    engine.shutdown();
}

// =================================================================
//  Renderer disabled -- renderer_active is false but extraction works
// =================================================================

static void test_renderer_disabled() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "renderer disabled: init ok");

    engine.step_one_frame();

    const auto& rf = engine.render_frame();
    check(rf.extracted_count > 0,
          "renderer disabled: extraction still works");

    // Telemetry should be populated (render phase is a no-op but timed).
    const auto& tel = engine.frame_telemetry();
    check(tel.total_frame_s >= 0.0,
          "renderer disabled: telemetry populated");

    engine.shutdown();
}

// =================================================================
//  Double shutdown -- shutdown() must be idempotent
// =================================================================

static void test_double_shutdown() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "double shutdown: init ok");

    engine.step_one_frame();
    engine.shutdown();
    engine.shutdown();   // must not crash

    check(true, "double shutdown: survived");
}

// =================================================================
//  Sim state accessible after init
// =================================================================

static void test_sim_state_after_init() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "sim state: init ok");

    auto snap0 = engine.sim_state().snapshot();
    check(snap0.tick_count == 0,
          "sim state: tick_count == 0 before step");
    check(snap0.entity_count > 0,
          "sim state: entities present after bootstrap");

    engine.shutdown();
}

// =================================================================
//  Single-step: exactly 1 tick per N press
// =================================================================

static void test_single_step_exact_tick() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "single-step: init ok");

    // Run several frames to let clock accumulate real deltas.
    for (int i = 0; i < 5; ++i)
        engine.step_one_frame();
    uint64_t tick_before_pause = engine.frame_info().sim_tick_index;

    // Pause.
    de::DebugAction pause = {};
    pause.toggle_pause = true;
    engine.debug_controls_mut().apply(pause);
    check(engine.debug_controls().paused, "single-step: paused");

    // Step once -- should advance exactly 1 tick.
    de::DebugAction step = {};
    step.single_step = true;
    engine.debug_controls_mut().apply(step);
    engine.step_one_frame();
    uint64_t tick_after_step1 = engine.frame_info().sim_tick_index;
    check(tick_after_step1 == tick_before_pause + 1,
          "single-step: exactly +1 tick after 1st N");

    // Step again -- another +1.
    engine.debug_controls_mut().apply(step);
    engine.step_one_frame();
    uint64_t tick_after_step2 = engine.frame_info().sim_tick_index;
    check(tick_after_step2 == tick_before_pause + 2,
          "single-step: exactly +2 ticks after 2nd N");

    // No step request -- tick should NOT advance.
    engine.step_one_frame();
    uint64_t tick_no_step = engine.frame_info().sim_tick_index;
    check(tick_no_step == tick_after_step2,
          "single-step: no advance without N");

    engine.shutdown();
}

// =================================================================
//  Pause prevents ticking
// =================================================================

static void test_pause_stops_ticking() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "pause-stop: init ok");

    engine.step_one_frame();
    uint64_t tick0 = engine.frame_info().sim_tick_index;

    // Pause.
    de::DebugAction pause = {};
    pause.toggle_pause = true;
    engine.debug_controls_mut().apply(pause);

    // Several frames while paused.
    for (int i = 0; i < 5; ++i)
        engine.step_one_frame();

    uint64_t tick_after_pause = engine.frame_info().sim_tick_index;
    check(tick_after_pause == tick0,
          "pause-stop: tick unchanged after 5 paused frames");

    engine.shutdown();
}

// =================================================================
//  Reset scene clears fixed-step accumulator
// =================================================================

static void test_reset_clears_accumulator() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "reset-accum: init ok");

    // Run several frames so the accumulator may hold partial residual time.
    for (int i = 0; i < 10; ++i)
        engine.step_one_frame();

    // Trigger reset via debug action.
    de::DebugAction reset = {};
    reset.reset_scene = true;
    engine.debug_controls_mut().apply(reset);
    engine.step_one_frame();

    // After reset, accumulator must be clean (no residual from prev scene).
    // The only accumulation is from the tiny clock delta of this one frame.
    const auto& fi = engine.frame_info();
    check(fi.frame_index == 1, "reset-accum: frame_index == 1 after reset");
    check(fi.steps_this_frame <= 2,
          "reset-accum: no burst of ticks after reset");

    // Pause immediately, then check accumulator is near-zero.
    de::DebugAction pause = {};
    pause.toggle_pause = true;
    engine.debug_controls_mut().apply(pause);
    engine.step_one_frame();  // paused frame: no consume, just clock update

    // The accumulator should be very small (only real clock deltas).
    check(engine.fixed_step().accumulator() < engine.fixed_step().step_dt(),
          "reset-accum: accumulator < step_dt after reset");

    engine.shutdown();
}

// =================================================================
//  Switch scene clears fixed-step accumulator
// =================================================================

static void test_switch_clears_accumulator() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "switch-accum: init ok");

    // Run several frames.
    for (int i = 0; i < 10; ++i)
        engine.step_one_frame();

    // Switch to Basic scene.
    de::DebugAction sw = {};
    sw.switch_scene = 0;  // Basic
    engine.debug_controls_mut().apply(sw);
    engine.step_one_frame();

    const auto& fi = engine.frame_info();
    check(fi.frame_index == 1, "switch-accum: frame_index == 1 after switch");
    check(fi.steps_this_frame <= 2,
          "switch-accum: no burst of ticks after switch");

    // Verify accumulator is clean after switch.
    check(engine.fixed_step().accumulator() < engine.fixed_step().step_dt(),
          "switch-accum: accumulator < step_dt after switch");

    engine.shutdown();
}

// =================================================================
//  Reset then single-step: predictable tick count
// =================================================================

static void test_reset_then_single_step() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "reset-step: init ok");

    // Run, pause, reset, then single-step.
    for (int i = 0; i < 5; ++i)
        engine.step_one_frame();

    de::DebugAction pause = {};
    pause.toggle_pause = true;
    engine.debug_controls_mut().apply(pause);

    de::DebugAction reset = {};
    reset.reset_scene = true;
    engine.debug_controls_mut().apply(reset);
    engine.step_one_frame();  // processes reset while paused

    // Now single-step: must produce exactly 1 tick from tick 0.
    de::DebugAction step = {};
    step.single_step = true;
    engine.debug_controls_mut().apply(step);
    engine.step_one_frame();

    check(engine.frame_info().sim_tick_index == 1,
          "reset-step: exactly 1 tick after reset + single-step");

    engine.shutdown();
}

// =================================================================
//  render_frame() is pristine pre-cull even after manual zoom/pan
// =================================================================

static void test_render_frame_pre_cull_after_zoom() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "pre-cull: init ok");

    // Run a frame in auto-frame mode to populate agents.
    engine.step_one_frame();
    const auto& rf0 = engine.render_frame();
    uint32_t agent_count = rf0.agent_count;
    uint32_t extracted0  = rf0.extracted_count;
    check(extracted0 > 0,             "pre-cull: agents present");
    check(extracted0 == agent_count,  "pre-cull: all extracted initially");

    // Switch to manual camera far from the crowd -> forces culling.
    de::DebugAction af = {};
    af.toggle_auto_frame = true;
    engine.debug_controls_mut().apply(af);
    auto& dc = engine.debug_controls_mut();
    dc.manual_center_x = 9999.0f;
    dc.manual_center_y = 9999.0f;
    dc.manual_hw       = 1.0f;

    engine.step_one_frame();

    // render_frame() must still be the full pre-cull extraction.
    const auto& rf1 = engine.render_frame();
    check(rf1.extracted_count == agent_count,
          "pre-cull: extracted_count unchanged after zoom");
    check(rf1.agent_count == agent_count,
          "pre-cull: agent_count unchanged after zoom");

    // But render_stats() must show culling.
    const auto& st = engine.render_stats();
    check(st.extracted_count == agent_count,
          "pre-cull: stats.extracted matches frame");
    check(st.visible_count == 0,
          "pre-cull: all agents culled when camera is far");
    check(st.culled_count == agent_count,
          "pre-cull: culled_count == agent_count");

    engine.shutdown();
}

// =================================================================
//  RenderStats culling invariant: visible + culled == extracted
// =================================================================

static void test_culling_stats_invariant() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "cull-inv: init ok");

    // Auto-frame: all visible, 0 culled.
    engine.step_one_frame();
    const auto& st0 = engine.render_stats();
    check(st0.visible_count + st0.culled_count == st0.extracted_count,
          "cull-inv: vis+cull==extracted (auto-frame)");
    check(st0.culled_count == 0,
          "cull-inv: 0 culled in auto-frame");

    // Manual camera far away: 0 visible, all culled.
    de::DebugAction af = {};
    af.toggle_auto_frame = true;
    engine.debug_controls_mut().apply(af);
    engine.debug_controls_mut().manual_center_x = 9999.0f;
    engine.debug_controls_mut().manual_center_y = 9999.0f;
    engine.debug_controls_mut().manual_hw       = 1.0f;

    engine.step_one_frame();
    const auto& st1 = engine.render_stats();
    check(st1.visible_count + st1.culled_count == st1.extracted_count,
          "cull-inv: vis+cull==extracted (far camera)");
    check(st1.visible_count == 0,
          "cull-inv: 0 visible with far camera");

    engine.shutdown();
}

// =================================================================
//  Overlay counters match RenderStats
// =================================================================

static void test_overlay_matches_render_stats() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "ov-match: init ok");

    engine.step_one_frame();

    const auto& st = engine.render_stats();
    const auto& ov = engine.debug_overlay();

    // Overlay line 4 = "Vis: N  cull:M  cap:K"
    // cap = agent_count - extracted_count (extraction-cap overflow).
    // This is NOT RenderStats.dropped_count (which is agent_count - instance_count).
    char expected[64];
    std::snprintf(expected, sizeof(expected), "Vis: %u  cull:%u  cap:%u",
                  st.visible_count, st.culled_count,
                  st.agent_count - st.extracted_count);
    check(std::strstr(ov.lines[4], expected) != nullptr,
          "ov-match: overlay counters coherent");

    engine.shutdown();
}

// =================================================================
//  Pause -> single-step -> resume: accumulator stays clean
// =================================================================

static void test_pause_step_resume() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "psr: init ok");

    // Run a few frames to reach steady state.
    for (int i = 0; i < 5; ++i)
        engine.step_one_frame();
    uint64_t tick_before = engine.frame_info().sim_tick_index;

    // Pause.
    de::DebugAction pause = {};
    pause.toggle_pause = true;
    engine.debug_controls_mut().apply(pause);

    // Several paused frames -- tick must not advance.
    for (int i = 0; i < 5; ++i)
        engine.step_one_frame();
    check(engine.frame_info().sim_tick_index == tick_before,
          "psr: tick unchanged while paused");

    // Single-step: exactly +1.
    de::DebugAction step = {};
    step.single_step = true;
    engine.debug_controls_mut().apply(step);
    engine.step_one_frame();
    check(engine.frame_info().sim_tick_index == tick_before + 1,
          "psr: +1 tick after single-step");

    // Resume (unpause).
    de::DebugAction unpause = {};
    unpause.toggle_pause = true;
    engine.debug_controls_mut().apply(unpause);
    check(!engine.debug_controls().paused, "psr: unpaused");

    // Run a frame after resume -- should NOT produce a burst of many ticks.
    engine.step_one_frame();
    uint64_t tick_after_resume = engine.frame_info().sim_tick_index;
    // At most a small number of steps (real clock delta is tiny in tests).
    check(tick_after_resume <= tick_before + 1 + 3,
          "psr: no tick burst after resume (at most ~2 steps)");

    engine.shutdown();
}

// =================================================================

int main() {
    test_crowd_headless();
    test_basic_headless();
    test_battlefield_headless();
    test_multi_frame();
    test_renderer_disabled();
    test_double_shutdown();
    test_sim_state_after_init();
    test_single_step_exact_tick();
    test_pause_stops_ticking();
    test_reset_clears_accumulator();
    test_switch_clears_accumulator();
    test_reset_then_single_step();
    test_render_frame_pre_cull_after_zoom();
    test_culling_stats_invariant();
    test_overlay_matches_render_stats();
    test_pause_step_resume();

    std::printf("\nEngineRuntimeTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
