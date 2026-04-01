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
//  Headless crowd: stats published after step_one_frame
// =================================================================

static void test_headless_stats_published() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "headless: init ok");

    engine.step_one_frame();

    const auto& st = engine.render_stats();
    check(st.agent_count > 0,        "headless: agent_count > 0");
    check(st.extracted_count > 0,    "headless: extracted_count > 0");
    check(st.instance_count == 0,    "headless: instance_count == 0 (no renderer)");
    check(st.draw_call_count == 0,   "headless: draw_call_count == 0");
    check(st.dropped_count == st.agent_count,
          "headless: dropped == agent_count (no renderer)");
    check(!st.frame_skipped,         "headless: frame_skipped == false");

    engine.shutdown();
}

// =================================================================
//  Headless: invariant holds across multiple frames
// =================================================================

static void test_headless_invariant_multiframe() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "multiframe: init ok");

    for (int i = 0; i < 5; ++i) {
        engine.step_one_frame();
        const auto& st = engine.render_stats();
        bool ok = (st.instance_count + st.dropped_count == st.agent_count);
        check(ok, "multiframe: invariant holds each frame");
    }

    engine.shutdown();
}

// =================================================================
//  Headless basic scene: 0 agents -> all stats zero
// =================================================================

static void test_headless_empty() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Basic;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "empty: init ok");

    engine.step_one_frame();

    const auto& st = engine.render_stats();
    check(st.agent_count == 0,       "empty: agent_count == 0");
    check(st.extracted_count == 0,   "empty: extracted_count == 0");
    check(st.instance_count == 0,    "empty: instance_count == 0");
    check(st.dropped_count == 0,     "empty: dropped == 0");
    check(st.draw_call_count == 0,   "empty: draw_call_count == 0");

    engine.shutdown();
}

// =================================================================
//  Stats not stale after init (before first frame)
// =================================================================

static void test_stats_fresh_after_init() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "fresh: init ok");

    // Before any step, stats should be default-initialized (all zero).
    const auto& st = engine.render_stats();
    check(st.agent_count == 0,       "fresh: agent_count == 0 before step");
    check(st.instance_count == 0,    "fresh: instance_count == 0 before step");
    check(st.draw_call_count == 0,   "fresh: draw_call_count == 0 before step");
    check(st.dropped_count == 0,     "fresh: dropped_count == 0 before step");
    check(!st.frame_skipped,         "fresh: frame_skipped == false before step");

    engine.shutdown();
}

// =================================================================
//  Agent count matches RenderFrame agent_count
// =================================================================

static void test_stats_match_render_frame() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "match: init ok");

    engine.step_one_frame();

    const auto& rf = engine.render_frame();
    const auto& st = engine.render_stats();

    check(st.agent_count == rf.agent_count,
          "match: stats.agent_count == rf.agent_count");
    check(st.extracted_count == rf.extracted_count,
          "match: stats.extracted_count == rf.extracted_count");

    engine.shutdown();
}

// =================================================================
//  Reinit same Engine: stats fresh after shutdown + init
// =================================================================

static void test_reinit_stats_fresh() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "reinit: first init ok");

    // Run a few frames to populate stats.
    for (int i = 0; i < 3; ++i)
        engine.step_one_frame();

    const auto& st1 = engine.render_stats();
    check(st1.agent_count > 0, "reinit: stats populated after first run");

    engine.shutdown();

    // After shutdown, stats must be clean.
    const auto& st2 = engine.render_stats();
    check(st2.agent_count == 0,     "reinit: agent_count == 0 after shutdown");
    check(st2.instance_count == 0,  "reinit: instance_count == 0 after shutdown");
    check(st2.dropped_count == 0,   "reinit: dropped_count == 0 after shutdown");
    check(st2.draw_call_count == 0, "reinit: draw_call_count == 0 after shutdown");
    check(!st2.frame_skipped,       "reinit: frame_skipped == false after shutdown");

    // Re-init with a different scene.
    cfg.start_scene = de::StartScene::Basic;
    check(engine.init(cfg), "reinit: second init ok");

    // Before stepping, stats must still be clean (not stale from first run).
    const auto& st3 = engine.render_stats();
    check(st3.agent_count == 0,     "reinit: agent_count == 0 after re-init");
    check(st3.instance_count == 0,  "reinit: instance_count == 0 after re-init");
    check(st3.dropped_count == 0,   "reinit: dropped_count == 0 after re-init");
    check(st3.draw_call_count == 0, "reinit: draw_call_count == 0 after re-init");

    // Step and verify new scene stats are correct (Basic = 0 agents).
    engine.step_one_frame();
    const auto& st4 = engine.render_stats();
    check(st4.agent_count == 0,     "reinit: basic scene agent_count == 0");
    check(st4.dropped_count == 0,   "reinit: basic scene dropped == 0");

    engine.shutdown();
}

// =================================================================
//  Reinit same Engine: crowd -> crowd, stats reflect new scene
// =================================================================

static void test_reinit_crowd_to_crowd() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "c2c: first init ok");

    engine.step_one_frame();
    uint32_t count1 = engine.render_stats().agent_count;
    check(count1 > 0, "c2c: first run has agents");

    engine.shutdown();

    // Re-init same scene.
    check(engine.init(cfg), "c2c: second init ok");

    // Fresh before step.
    check(engine.render_stats().agent_count == 0,
          "c2c: fresh after re-init");

    engine.step_one_frame();
    uint32_t count2 = engine.render_stats().agent_count;
    check(count2 > 0, "c2c: second run has agents");

    engine.shutdown();
}

// =================================================================

int main() {
    test_headless_stats_published();
    test_headless_invariant_multiframe();
    test_headless_empty();
    test_stats_fresh_after_init();
    test_stats_match_render_frame();
    test_reinit_stats_fresh();
    test_reinit_crowd_to_crowd();

    std::printf("\nRenderStatsTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
