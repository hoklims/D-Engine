#include "Runtime/Engine.h"

#include <cstdio>

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

int main() {
    test_crowd_headless();
    test_basic_headless();
    test_battlefield_headless();
    test_multi_frame();
    test_renderer_disabled();
    test_double_shutdown();
    test_sim_state_after_init();

    std::printf("\nEngineRuntimeTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
