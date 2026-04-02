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
    check(st.draw_call_count == 0,            "headless: draw_call_count == 0");
    check(st.overlay_draw_call_count == 0,  "headless: overlay_draw_call_count == 0");
    check(st.dropped_count == st.agent_count,
          "headless: dropped == agent_count (no renderer)");
    check(!st.frame_skipped,                "headless: frame_skipped == false");

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
//  Failed init: stats clean even when init() returns false
// =================================================================

static void test_failed_init_stats_clean() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "fail-init: first init ok");

    // Populate stats with a real frame.
    engine.step_one_frame();
    check(engine.render_stats().agent_count > 0,
          "fail-init: stats populated");

    engine.shutdown();

    // Now attempt init with invalid config (sim_rate_hz = 0 -> FixedStep
    // rejects it). init() must return false AND stats must be clean.
    de::EngineConfig bad_cfg;
    bad_cfg.sim_rate_hz          = 0.0;
    bad_cfg.enable_renderer      = false;
    check(!engine.init(bad_cfg), "fail-init: bad config rejected");

    const auto& st = engine.render_stats();
    check(st.agent_count == 0,     "fail-init: agent_count == 0");
    check(st.extracted_count == 0, "fail-init: extracted_count == 0");
    check(st.instance_count == 0,  "fail-init: instance_count == 0");
    check(st.dropped_count == 0,   "fail-init: dropped_count == 0");
    check(st.draw_call_count == 0, "fail-init: draw_call_count == 0");
    check(!st.frame_skipped,       "fail-init: frame_skipped == false");
}

// =================================================================
//  draw_call_count invariant: world + crowd
// =================================================================

static void test_draw_call_invariant_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "dc-inv: init ok");

    engine.step_one_frame();

    const auto& st = engine.render_stats();
    // Headless: no draw calls at all.
    uint32_t crowd_dc = (st.instance_count > 0) ? 1u : 0u;
    check(st.draw_call_count == st.world_draw_call_count + crowd_dc
                              + st.overlay_draw_call_count,
          "dc-inv: draw_call == world + crowd + overlay (headless)");
    check(st.world_draw_call_count == 0,
          "dc-inv: world_draw_call == 0 (headless)");
    check(st.overlay_draw_call_count == 0,
          "dc-inv: overlay_draw_call == 0 (headless)");

    engine.shutdown();
}

static void test_draw_call_invariant_empty_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Basic;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "dc-empty: init ok");

    engine.step_one_frame();

    const auto& st = engine.render_stats();
    check(st.draw_call_count == 0,
          "dc-empty: draw_call == 0 (empty headless)");
    check(st.world_draw_call_count == 0,
          "dc-empty: world_draw_call == 0 (empty headless)");
    check(st.overlay_draw_call_count == 0,
          "dc-empty: overlay_draw_call == 0 (empty headless)");

    engine.shutdown();
}

// =================================================================
//  Overlay data uses current frame, not previous
// =================================================================

static void test_overlay_current_frame_coherence() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "overlay-coh: init ok");

    // First frame: no previous render_stats_ exist yet.
    // If the overlay used stale stats, drawn/agent would be 0.
    engine.step_one_frame();

    const auto& rf = engine.render_frame();
    const auto& ov = engine.debug_overlay();

    check(rf.agent_count > 0, "overlay-coh: agents exist");

    // Overlay line 3 = "Agents: N"  -- must show current agent_count.
    char expected_agents[32];
    std::snprintf(expected_agents, sizeof(expected_agents), "%u",
                  rf.agent_count);
    check(std::strstr(ov.lines[3], expected_agents) != nullptr,
          "overlay-coh: agent_count matches current frame");

    // Overlay line 4 = "Vis: N  cull:M  cap:K".
    // In auto-frame headless, visible == extracted (no culling).
    const auto& st = engine.render_stats();
    char expected_vis[32];
    std::snprintf(expected_vis, sizeof(expected_vis), "Vis: %u",
                  st.visible_count);
    check(std::strstr(ov.lines[4], expected_vis) != nullptr,
          "overlay-coh: Vis matches render_stats visible_count");

    engine.shutdown();
}

// =================================================================
//  Overlay: headless shows "Render: OFF", not "SKIPPED"
// =================================================================

static void test_overlay_renderer_off_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "overlay-off: init ok");

    engine.step_one_frame();

    const auto& ov = engine.debug_overlay();

    // Headless: renderer never created.  HUD/overlay show "Render: OFF"
    // (not SKIPPED, which is reserved for actual frame loss).
    bool found_off     = false;
    bool found_skipped = false;
    for (int i = 0; i < ov.line_count; ++i) {
        if (std::strstr(ov.lines[i], "Render: OFF")) found_off = true;
        if (std::strstr(ov.lines[i], "SKIPPED"))     found_skipped = true;
    }
    check(found_off,      "overlay-off: Render: OFF shown in headless");
    check(!found_skipped, "overlay-off: no SKIPPED in headless");

    engine.shutdown();
}

// =================================================================
//  draw_call_count invariant across multiple frames
// =================================================================

static void test_draw_call_invariant_multiframe_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "dc-multi: init ok");

    for (int i = 0; i < 5; ++i) {
        engine.step_one_frame();
        const auto& st = engine.render_stats();
        uint32_t crowd_dc = (st.instance_count > 0) ? 1u : 0u;
        bool inv = (st.draw_call_count ==
                    st.world_draw_call_count + crowd_dc
                  + st.overlay_draw_call_count);
        check(inv, "dc-multi: invariant holds each frame");
    }

    engine.shutdown();
}

// =================================================================
//  Overlay lifecycle: fresh after init, clean after shutdown/fail
// =================================================================

static void test_overlay_fresh_after_init() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "ov-fresh: init ok");

    const auto& ov = engine.debug_overlay();
    check(ov.line_count == 0, "ov-fresh: line_count == 0 before step");

    engine.shutdown();
}

static void test_overlay_clean_after_shutdown() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "ov-shut: init ok");

    engine.step_one_frame();
    check(engine.debug_overlay().line_count > 0,
          "ov-shut: populated after step");

    engine.shutdown();
    check(engine.debug_overlay().line_count == 0,
          "ov-shut: line_count == 0 after shutdown");
}

static void test_overlay_clean_after_failed_init() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "ov-fail: first init ok");

    engine.step_one_frame();
    check(engine.debug_overlay().line_count > 0,
          "ov-fail: populated after step");

    engine.shutdown();

    // Bad config: sim_rate_hz = 0 -> FixedStep rejects.
    de::EngineConfig bad;
    bad.sim_rate_hz     = 0.0;
    bad.enable_renderer = false;
    check(!engine.init(bad), "ov-fail: bad config rejected");

    check(engine.debug_overlay().line_count == 0,
          "ov-fail: clean after failed init");
}

static void test_overlay_clean_after_reinit() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "ov-reinit: first init ok");

    engine.step_one_frame();
    check(engine.debug_overlay().line_count > 0,
          "ov-reinit: populated after step");

    engine.shutdown();

    cfg.start_scene = de::StartScene::Basic;
    check(engine.init(cfg), "ov-reinit: second init ok");
    check(engine.debug_overlay().line_count == 0,
          "ov-reinit: clean after reinit before step");

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
    test_failed_init_stats_clean();
    test_draw_call_invariant_headless();
    test_draw_call_invariant_empty_headless();
    test_overlay_current_frame_coherence();
    test_overlay_renderer_off_headless();
    test_draw_call_invariant_multiframe_headless();
    test_overlay_fresh_after_init();
    test_overlay_clean_after_shutdown();
    test_overlay_clean_after_failed_init();
    test_overlay_clean_after_reinit();

    std::printf("\nRenderStatsTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
