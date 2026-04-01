#include "Runtime/DemoPresets.h"
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
//  Preset table sanity
// =================================================================

static void test_preset_table_valid() {
    check(de::k_demo_preset_count == 4, "table: 4 presets");
    for (int i = 0; i < de::k_demo_preset_count; ++i) {
        const auto& p = de::k_demo_presets[i];
        check(p.name != nullptr,            "table: name not null");
        check(p.name[0] != '\0',            "table: name not empty");
        check(p.crowd.agents_per_team > 0,  "table: agents > 0");
        check(p.crowd.health > 0.0f,        "table: health > 0");
        check(p.camera_hw > 0.0f,           "table: camera_hw > 0");
    }
}

// =================================================================
//  preset_name()
// =================================================================

static void test_preset_name() {
    check(de::preset_name(-1) == nullptr,  "name: -1 -> null");
    check(de::preset_name(4) == nullptr,   "name: 4 -> null");
    check(std::strcmp(de::preset_name(0), "LaneClash") == 0,
          "name: 0 -> LaneClash");
    check(std::strcmp(de::preset_name(1), "DenseMelee") == 0,
          "name: 1 -> DenseMelee");
    check(std::strcmp(de::preset_name(2), "WallGap") == 0,
          "name: 2 -> WallGap");
    check(std::strcmp(de::preset_name(3), "SparseApproach") == 0,
          "name: 3 -> SparseApproach");
}

// =================================================================
//  apply_demo_preset: each preset bootstraps correctly
// =================================================================

static void test_apply_each_preset() {
    for (int i = 0; i < de::k_demo_preset_count; ++i) {
        const auto& p = de::k_demo_presets[i];
        de::SimState sim;
        de::apply_demo_preset(sim, p);

        auto snap = sim.snapshot();
        int expected = p.crowd.agents_per_team * 2;
        char label[64];

        std::snprintf(label, sizeof(label), "apply[%d]: entity_count", i);
        check(snap.entity_count == static_cast<uint32_t>(expected), label);

        std::snprintf(label, sizeof(label), "apply[%d]: crowd_agent_count", i);
        check(snap.crowd_agent_count == static_cast<uint32_t>(expected), label);

        std::snprintf(label, sizeof(label), "apply[%d]: system_count > 0", i);
        check(snap.system_count > 0, label);

        sim.shutdown();
    }
}

// =================================================================
//  WallGap uses Battlefield bootstrap (nav grid active)
// =================================================================

static void test_wall_gap_has_nav() {
    const auto& p = de::k_demo_presets[2];  // WallGap
    check(p.scene_type == de::StartScene::Battlefield,
          "wallgap: scene_type == Battlefield");
    check(p.bf_obstacles != nullptr,  "wallgap: has obstacles");
    check(p.bf_obstacle_count > 0,    "wallgap: obstacle_count > 0");

    de::SimState sim;
    de::apply_demo_preset(sim, p);
    auto snap = sim.snapshot();
    check(snap.nav_blocked_cells > 0, "wallgap: nav has blocked cells");
    sim.shutdown();
}

// =================================================================
//  Crowd presets have no nav
// =================================================================

static void test_crowd_presets_no_nav() {
    for (int i : {0, 1, 3}) {
        const auto& p = de::k_demo_presets[i];
        check(p.scene_type == de::StartScene::Crowd,
              "crowd-preset: scene_type == Crowd");

        de::SimState sim;
        de::apply_demo_preset(sim, p);
        auto snap = sim.snapshot();
        char label[64];
        std::snprintf(label, sizeof(label),
                      "crowd[%d]: nav_blocked_cells == 0", i);
        check(snap.nav_blocked_cells == 0, label);
        sim.shutdown();
    }
}

// =================================================================
//  Preset differentiation: each preset produces different agent count
// =================================================================

static void test_presets_differentiated() {
    uint32_t counts[4] = {};
    for (int i = 0; i < 4; ++i) {
        de::SimState sim;
        de::apply_demo_preset(sim, de::k_demo_presets[i]);
        counts[i] = sim.snapshot().crowd_agent_count;
        sim.shutdown();
    }
    // All 4 presets have different agent counts.
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            char label[64];
            std::snprintf(label, sizeof(label),
                          "diff: preset %d != preset %d", i, j);
            check(counts[i] != counts[j], label);
        }
    }
}

// =================================================================
//  Engine: init with default preset (no-arg) -> LaneClash
// =================================================================

static void test_engine_default_preset() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 0;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "engine-default: init ok");
    check(engine.current_preset() == 0, "engine-default: preset == 0");
    check(std::strcmp(engine.current_scene_label(), "LaneClash") == 0,
          "engine-default: label == LaneClash");

    engine.step_one_frame();
    check(engine.render_stats().agent_count == 80,
          "engine-default: 80 agents (40x2)");

    engine.shutdown();
}

// =================================================================
//  Engine: init with demo_preset = -1 falls back to start_scene
// =================================================================

static void test_engine_no_preset_fallback() {
    de::EngineConfig cfg;
    cfg.demo_preset     = -1;
    cfg.start_scene     = de::StartScene::Basic;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "fallback: init ok");
    check(engine.current_preset() == -1, "fallback: preset == -1");
    check(std::strcmp(engine.current_scene_label(), "Basic") == 0,
          "fallback: label == Basic");

    engine.step_one_frame();
    check(engine.render_stats().agent_count == 0,
          "fallback: 0 agents (Basic scene)");

    engine.shutdown();
}

// =================================================================
//  Engine: runtime preset switch via DebugAction
// =================================================================

static void test_engine_preset_switch() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 0;  // LaneClash
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "switch: init ok");
    engine.step_one_frame();
    uint32_t count0 = engine.render_stats().agent_count;
    check(count0 == 80, "switch: LaneClash has 80 agents");

    // Switch to DenseMelee (preset 1) via debug action.
    de::DebugAction sw = {};
    sw.switch_preset = 1;
    engine.debug_controls_mut().apply(sw);
    engine.step_one_frame();

    check(engine.current_preset() == 1, "switch: preset == 1");
    check(std::strcmp(engine.current_scene_label(), "DenseMelee") == 0,
          "switch: label == DenseMelee");
    check(engine.render_stats().agent_count == 120,
          "switch: DenseMelee has 120 agents");

    // Switch to SparseApproach (preset 3).
    de::DebugAction sw2 = {};
    sw2.switch_preset = 3;
    engine.debug_controls_mut().apply(sw2);
    engine.step_one_frame();

    check(engine.current_preset() == 3, "switch: preset == 3");
    check(engine.render_stats().agent_count == 24,
          "switch: SparseApproach has 24 agents");

    engine.shutdown();
}

// =================================================================
//  Engine: reset re-bootstraps current preset
// =================================================================

static void test_engine_preset_reset() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 1;  // DenseMelee
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "reset: init ok");

    // Run several frames (agents die, count changes).
    for (int i = 0; i < 10; ++i)
        engine.step_one_frame();

    // Reset.
    de::DebugAction reset = {};
    reset.reset_scene = true;
    engine.debug_controls_mut().apply(reset);
    engine.step_one_frame();

    // After reset, still on DenseMelee with fresh 120 agents.
    check(engine.current_preset() == 1, "reset: still preset 1");
    check(engine.render_stats().agent_count == 120,
          "reset: 120 agents after reset");

    engine.shutdown();
}

// =================================================================
//  Engine: scene_switch (legacy) clears current_preset
// =================================================================

static void test_engine_scene_switch_clears_preset() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 0;  // LaneClash
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "legacy: init ok");
    check(engine.current_preset() == 0, "legacy: starts on preset 0");

    // Switch to Basic scene via legacy scene_switch.
    de::DebugAction sw = {};
    sw.switch_scene = 0;  // Basic
    engine.debug_controls_mut().apply(sw);
    engine.step_one_frame();

    check(engine.current_preset() == -1,
          "legacy: preset cleared after scene switch");
    check(std::strcmp(engine.current_scene_label(), "Basic") == 0,
          "legacy: label == Basic");

    engine.shutdown();
}

// =================================================================
//  DebugControls: preset_switch apply + consume
// =================================================================

static void test_debug_controls_preset_switch() {
    de::DebugControls dc;
    check(dc.preset_switch == -1, "dc: default preset_switch == -1");

    de::DebugAction a = {};
    a.switch_preset = 2;
    dc.apply(a);
    check(dc.preset_switch == 2, "dc: preset_switch == 2 after apply");

    dc.consume();
    check(dc.preset_switch == -1, "dc: preset_switch == -1 after consume");
}

// =================================================================
//  Engine: overlay shows preset name on first frame
// =================================================================

static void test_overlay_shows_preset_name() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 0;  // LaneClash
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "overlay-name: init ok");

    engine.step_one_frame();

    const auto& ov = engine.debug_overlay();
    check(ov.line_count > 0, "overlay-name: has lines");
    // First overlay line should contain "LaneClash".
    bool found = false;
    for (int i = 0; i < ov.line_count; ++i) {
        if (std::strstr(ov.lines[i], "LaneClash")) {
            found = true;
            break;
        }
    }
    check(found, "overlay-name: LaneClash in overlay");

    engine.shutdown();
}

// =================================================================

int main() {
    test_preset_table_valid();
    test_preset_name();
    test_apply_each_preset();
    test_wall_gap_has_nav();
    test_crowd_presets_no_nav();
    test_presets_differentiated();
    test_engine_default_preset();
    test_engine_no_preset_fallback();
    test_engine_preset_switch();
    test_engine_preset_reset();
    test_engine_scene_switch_clears_preset();
    test_debug_controls_preset_switch();
    test_overlay_shows_preset_name();

    std::printf("\nDemoPresetTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
