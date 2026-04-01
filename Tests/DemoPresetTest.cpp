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
//  Lifecycle: current_preset clean after shutdown
// =================================================================

static void test_current_preset_clean_after_shutdown() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 1;  // DenseMelee
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "shut-preset: init ok");
    check(engine.current_preset() == 1, "shut-preset: preset == 1");

    engine.shutdown();
    check(engine.current_preset() == -1,
          "shut-preset: preset == -1 after shutdown");
}

// =================================================================
//  Lifecycle: current_scene_label not stale after shutdown
// =================================================================

static void test_label_not_stale_after_shutdown() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 0;  // LaneClash
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "shut-label: init ok");
    check(std::strcmp(engine.current_scene_label(), "LaneClash") == 0,
          "shut-label: label == LaneClash before shutdown");

    engine.shutdown();
    // After shutdown, preset is cleared. Label falls back to start_scene
    // (default Crowd from EngineConfig).
    check(std::strcmp(engine.current_scene_label(), "Crowd") == 0,
          "shut-label: label == Crowd after shutdown (fallback)");
}

// =================================================================
//  Lifecycle: reinit from preset to legacy, world debug nominal
// =================================================================

static void test_reinit_preset_to_legacy_world_debug() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 1;  // DenseMelee (extent=30, spacing=5)
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "reinit-wdb: first init ok");

    // Verify preset's world debug config is applied.
    check(engine.world_debug_config().world_extent == 30.0f,
          "reinit-wdb: extent == 30 from DenseMelee");
    check(engine.world_debug_config().grid_spacing == 5.0f,
          "reinit-wdb: spacing == 5 from DenseMelee");

    engine.shutdown();

    // Re-init with legacy path (no preset).
    de::EngineConfig cfg2;
    cfg2.demo_preset     = -1;
    cfg2.start_scene     = de::StartScene::Crowd;
    cfg2.enable_renderer = false;

    check(engine.init(cfg2), "reinit-wdb: second init ok");

    // World debug must be nominal (100, 10), not stale from DenseMelee.
    check(engine.world_debug_config().world_extent == 100.0f,
          "reinit-wdb: extent == 100 after legacy reinit");
    check(engine.world_debug_config().grid_spacing == 10.0f,
          "reinit-wdb: spacing == 10 after legacy reinit");

    engine.shutdown();
}

// =================================================================
//  Lifecycle: scene_switch resets world debug after preset
// =================================================================

static void test_scene_switch_resets_world_debug() {
    de::EngineConfig cfg;
    cfg.demo_preset     = 1;  // DenseMelee (extent=30)
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "switch-wdb: init ok");
    engine.step_one_frame();

    check(engine.world_debug_config().world_extent == 30.0f,
          "switch-wdb: extent == 30 from DenseMelee");

    // Legacy scene switch to Basic.
    de::DebugAction sw = {};
    sw.switch_scene = 0;  // Basic
    engine.debug_controls_mut().apply(sw);
    engine.step_one_frame();

    // World debug must be nominal, not stale from DenseMelee.
    check(engine.world_debug_config().world_extent == 100.0f,
          "switch-wdb: extent == 100 after scene switch");
    check(engine.world_debug_config().grid_spacing == 10.0f,
          "switch-wdb: spacing == 10 after scene switch");

    engine.shutdown();
}

// =================================================================
//  Lifecycle: world_debug_config fresh on first init (no stale data)
// =================================================================

static void test_world_debug_fresh_on_init() {
    de::EngineConfig cfg;
    cfg.demo_preset     = -1;
    cfg.start_scene     = de::StartScene::Basic;
    cfg.enable_renderer = false;

    de::Engine engine;
    check(engine.init(cfg), "fresh-wdb: init ok");

    // Must be nominal defaults.
    check(engine.world_debug_config().world_extent == 100.0f,
          "fresh-wdb: extent == 100");
    check(engine.world_debug_config().grid_spacing == 10.0f,
          "fresh-wdb: spacing == 10");
    check(engine.world_debug_config().show_ground,
          "fresh-wdb: show_ground == true");
    check(engine.world_debug_config().show_grid,
          "fresh-wdb: show_grid == true");

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
    test_current_preset_clean_after_shutdown();
    test_label_not_stale_after_shutdown();
    test_reinit_preset_to_legacy_world_debug();
    test_scene_switch_resets_world_debug();
    test_world_debug_fresh_on_init();

    std::printf("\nDemoPresetTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
