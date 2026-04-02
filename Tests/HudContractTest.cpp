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
//  First frame: HUD populated with correct sections
// =================================================================

static void test_first_frame_hud_populated() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "first-hud: init ok");
    check(engine.debug_hud().section_count == 0,
          "first-hud: empty before step");

    engine.step_one_frame();

    const auto& hud = engine.debug_hud();
    check(hud.mode == de::HudMode::Full, "first-hud: Full mode");
    check(hud.section_count == 4,        "first-hud: 4 sections");

    check(std::strcmp(hud.sections[0].title, "RUNTIME")  == 0,
          "first-hud: s0 = RUNTIME");
    check(std::strcmp(hud.sections[1].title, "CROWD")    == 0,
          "first-hud: s1 = CROWD");
    check(std::strcmp(hud.sections[2].title, "BUDGET")   == 0,
          "first-hud: s2 = BUDGET");
    check(std::strcmp(hud.sections[3].title, "CONTROLS") == 0,
          "first-hud: s3 = CONTROLS");

    engine.shutdown();
}

// =================================================================
//  Sim timing reflects the CURRENT frame, not the previous one.
//
//  Strategy: run several frames (sim ticks happen), then pause.
//  On the paused frame, no sim tick runs -> wip_telemetry_.fixed_update_s = 0.
//  Old bug: telemetry_ (previous frame) would show non-zero sim time.
//  Fix: wip_telemetry_ (current frame) shows 0.0ms correctly.
// =================================================================

static void test_sim_timing_current_frame() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "sim-timing: init ok");

    // Run enough frames for the fixed-step accumulator to catch up
    // and produce real sim ticks with measurable time.
    for (int i = 0; i < 10; ++i)
        engine.step_one_frame();

    // Pause the simulation.
    de::DebugAction pause = {};
    pause.toggle_pause = true;
    engine.debug_controls_mut().apply(pause);
    check(engine.debug_controls().paused, "sim-timing: paused");

    // Run one paused frame.  No sim tick runs, so current-frame
    // fixed_update_s == 0.  The HUD must reflect THIS frame's sim
    // time, not the previous frame's.
    engine.step_one_frame();

    const auto& hud = engine.debug_hud();
    check(hud.section_count >= 3, "sim-timing: budget section exists");

    // Budget section: look for "Sim:0.0ms" confirming current-frame timing.
    bool found_zero_sim = false;
    for (int li = 0; li < hud.sections[2].line_count; ++li) {
        if (std::strstr(hud.sections[2].lines[li], "Sim:0.0ms"))
            found_zero_sim = true;
    }
    check(found_zero_sim,
          "sim-timing: paused frame shows Sim:0.0ms (current frame)");

    engine.shutdown();
}

// =================================================================
//  Frame timing uses current-frame pre-render sum, not previous total
//
//  Same strategy: on a paused frame the only CPU work is begin_frame
//  + update_presentation (no sim ticks), so the pre-render sum is
//  tiny.  With the old code, the previous frame's total_frame_s
//  would bleed through and show a larger number.
// =================================================================

static void test_frame_timing_current_frame() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "frame-timing: init ok");

    for (int i = 0; i < 10; ++i)
        engine.step_one_frame();

    de::DebugAction pause = {};
    pause.toggle_pause = true;
    engine.debug_controls_mut().apply(pause);
    engine.step_one_frame();

    const auto& hud = engine.debug_hud();
    // Budget section: timing line should exist with CPU: label.
    bool found_timing = false;
    for (int li = 0; li < hud.sections[2].line_count; ++li) {
        if (std::strstr(hud.sections[2].lines[li], "CPU:") &&
            std::strstr(hud.sections[2].lines[li], "Sim:"))
            found_timing = true;
    }
    check(found_timing, "frame-timing: CPU:/Sim: timing line present on paused frame");

    engine.shutdown();
}

// =================================================================
//  Headless: HUD shows "Render: OFF", not "SKIPPED"
// =================================================================

static void test_hud_renderer_off_headless() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "rend-off: init ok");

    engine.step_one_frame();

    const auto& hud = engine.debug_hud();
    bool found_off     = false;
    bool found_skipped = false;
    for (int si = 0; si < hud.section_count; ++si)
        for (int li = 0; li < hud.sections[si].line_count; ++li) {
            if (std::strstr(hud.sections[si].lines[li], "Render: OFF"))
                found_off = true;
            if (std::strstr(hud.sections[si].lines[li], "SKIPPED"))
                found_skipped = true;
        }

    check(found_off,      "rend-off: Render: OFF shown in headless HUD");
    check(!found_skipped, "rend-off: no SKIPPED in headless HUD");

    engine.shutdown();
}

// =================================================================
//  HUD vs render_stats: visible/culled coherent
// =================================================================

static void test_hud_coherent_with_stats() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "hud-stats: init ok");

    engine.step_one_frame();

    const auto& hud = engine.debug_hud();
    const auto& st  = engine.render_stats();

    // Crowd section (index 1) contains "Vis:N".
    char vis_str[32];
    std::snprintf(vis_str, sizeof(vis_str), "Vis:%u", st.visible_count);
    bool found = false;
    for (int li = 0; li < hud.sections[1].line_count; ++li)
        if (std::strstr(hud.sections[1].lines[li], vis_str))
            found = true;
    check(found, "hud-stats: visible count matches render_stats");

    // Culled count.
    char cul_str[32];
    std::snprintf(cul_str, sizeof(cul_str), "cull:%u", st.culled_count);
    bool found_cul = false;
    for (int li = 0; li < hud.sections[1].line_count; ++li)
        if (std::strstr(hud.sections[1].lines[li], cul_str))
            found_cul = true;
    check(found_cul, "hud-stats: culled count matches render_stats");

    engine.shutdown();
}

// =================================================================
//  Headless: HUD and render_stats agree -- no false SKIPPED
// =================================================================

static void test_headless_hud_and_stats_coherent() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "coherent: init ok");

    engine.step_one_frame();

    // render_stats.frame_skipped = false: headless is not a failure.
    check(!engine.render_stats().frame_skipped,
          "coherent: render_stats.frame_skipped == false (headless)");

    // HUD must NOT show SKIPPED (no frame was lost).
    // HUD shows "Render: OFF" instead (distinct signal).
    const auto& hud = engine.debug_hud();
    bool hud_skipped = false;
    bool hud_off     = false;
    for (int si = 0; si < hud.section_count; ++si)
        for (int li = 0; li < hud.sections[si].line_count; ++li) {
            if (std::strstr(hud.sections[si].lines[li], "SKIPPED"))
                hud_skipped = true;
            if (std::strstr(hud.sections[si].lines[li], "Render: OFF"))
                hud_off = true;
        }
    check(!hud_skipped, "coherent: HUD has no SKIPPED (headless != failure)");
    check(hud_off,      "coherent: HUD shows Render: OFF");

    engine.shutdown();
}

// =================================================================
//  HUD clean lifecycle: empty after init, populated after step,
//  empty after shutdown.
// =================================================================

static void test_hud_clean_lifecycle() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "lifecycle: init ok");
    check(engine.debug_hud().section_count == 0,
          "lifecycle: empty after init");

    engine.step_one_frame();
    check(engine.debug_hud().section_count > 0,
          "lifecycle: populated after step");

    engine.shutdown();
    check(engine.debug_hud().section_count == 0,
          "lifecycle: empty after shutdown");
}

// =================================================================
//  Multi-frame coherence: HUD tracks tick progression
// =================================================================

static void test_hud_tracks_tick() {
    de::EngineConfig cfg;
    cfg.start_scene     = de::StartScene::Crowd;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "tick-track: init ok");

    for (int i = 0; i < 5; ++i)
        engine.step_one_frame();

    const auto& hud = engine.debug_hud();
    uint64_t tick = engine.frame_info().sim_tick_index;

    // Runtime section should show current tick.
    char tick_str[32];
    std::snprintf(tick_str, sizeof(tick_str), "%llu",
                  static_cast<unsigned long long>(tick));
    bool found = false;
    for (int li = 0; li < hud.sections[0].line_count; ++li)
        if (std::strstr(hud.sections[0].lines[li], tick_str))
            found = true;
    check(found, "tick-track: HUD shows current tick");

    engine.shutdown();
}

// =================================================================

int main() {
    test_first_frame_hud_populated();
    test_sim_timing_current_frame();
    test_frame_timing_current_frame();
    test_hud_renderer_off_headless();
    test_hud_coherent_with_stats();
    test_headless_hud_and_stats_coherent();
    test_hud_clean_lifecycle();
    test_hud_tracks_tick();

    std::printf("\nHudContractTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
