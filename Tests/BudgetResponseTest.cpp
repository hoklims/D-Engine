#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"

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

// Helper: budget config that always violates (tick time always > 1 ps).
// Only tick_over fires; all other thresholds are generous.
static de::SimBudgetConfig always_over_budget() {
    de::SimBudgetConfig b{};
    b.max_tick_s            = 1e-12;  // 1 picosecond -- impossible to beat
    b.max_system_s          = 100.0;
    b.max_targeting_scanned = 999999;
    b.max_melee_checks      = 999999;
    b.max_lod_t0_count      = 999999;
    return b;
}

// =================================================================
//  Under budget: no degradation ever
// =================================================================

static void test_under_budget_no_degradation() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Enable response with default generous budget.
    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    for (int i = 0; i < 20; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget_pressure_level == 0,
          "under_budget: pressure stays 0");
    check(!snap.budget_response_active,
          "under_budget: response not active");
    check(snap.budget_lod_scale == 1.0f,
          "under_budget: lod_scale remains 1.0");
}

// =================================================================
//  Over budget: degradation activates
// =================================================================

static void test_over_budget_activates() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(snap.budget_pressure_level == 1,
          "over_budget: pressure increased to 1");
    check(snap.budget_response_active,
          "over_budget: response is active");
    check(snap.budget_lod_scale < 1.0f,
          "over_budget: lod_scale reduced");
}

// =================================================================
//  Pressure climbs progressively, bounded by max
// =================================================================

static void test_pressure_bounded() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled       = true;
    resp.max_pressure  = 4;
    sim.set_budget_response_config(resp);

    // Run 10 ticks -- pressure should cap at 4.
    for (int i = 0; i < 10; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget_pressure_level == 4,
          "bounded: pressure capped at max_pressure=4");
    check(snap.budget_lod_scale > 0.0f,
          "bounded: lod_scale stays positive");
    check(snap.budget_lod_scale <= 0.25f,
          "bounded: lod_scale at max pressure ~0.2");
}

// =================================================================
//  Progressive recovery with hysteresis
// =================================================================

static void test_progressive_recovery() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled         = true;
    resp.max_pressure    = 4;
    resp.recovery_ticks  = 3;
    resp.shrink_per_level = 0.20f;
    sim.set_budget_response_config(resp);

    // 4 ticks of violation -> pressure = 4.
    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 4,
          "recovery: pressure reached 4");

    // Relax budget -- all ticks now within budget.
    de::SimBudgetConfig generous{};
    generous.max_tick_s            = 100.0;
    generous.max_system_s          = 100.0;
    generous.max_targeting_scanned = 999999;
    generous.max_melee_checks      = 999999;
    generous.max_lod_t0_count      = 999999;
    sim.set_budget_config(generous);

    // 1 tick: still at pressure 4 (need 3 healthy ticks for -1).
    sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 4,
          "recovery: pressure still 4 after 1 healthy tick");

    // 2 more ticks (total 3 healthy) -> pressure drops to 3.
    sim.tick(1.0 / 60.0);
    sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 3,
          "recovery: pressure dropped to 3 after 3 healthy ticks");

    // 3 more healthy ticks -> pressure drops to 2.
    for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 2,
          "recovery: pressure dropped to 2 after 6 more healthy ticks");

    // Full recovery: 6 more healthy ticks -> pressure 0.
    for (int i = 0; i < 6; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 0,
          "recovery: pressure back to 0");
    check(!sim.budget_response_state().active,
          "recovery: response deactivated");
    check(sim.budget_response_state().lod_distance_scale == 1.0f,
          "recovery: lod_scale back to 1.0");
}

// =================================================================
//  Deterministic: two identical runs produce identical response
// =================================================================

static void test_deterministic_response() {
    auto run = [](de::SimState& sim) {
        de::CrowdConfig cfg{};
        cfg.agents_per_team = 10;
        sim.bootstrap_crowd(cfg);
        sim.set_budget_config(always_over_budget());

        de::SimBudgetResponseConfig resp{};
        resp.enabled = true;
        sim.set_budget_response_config(resp);

        // 3 ticks over budget, then relax, then 5 ticks.
        for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);

        de::SimBudgetConfig generous{};
        generous.max_tick_s            = 100.0;
        generous.max_system_s          = 100.0;
        generous.max_targeting_scanned = 999999;
        generous.max_melee_checks      = 999999;
        generous.max_lod_t0_count      = 999999;
        sim.set_budget_config(generous);

        for (int i = 0; i < 5; ++i) sim.tick(1.0 / 60.0);

        return sim.snapshot();
    };

    de::SimState sim1, sim2;
    auto snap1 = run(sim1);
    auto snap2 = run(sim2);

    check(snap1.budget_pressure_level == snap2.budget_pressure_level,
          "deterministic: pressure_level matches");
    check(snap1.budget_response_active == snap2.budget_response_active,
          "deterministic: response_active matches");
    check(snap1.budget_lod_scale == snap2.budget_lod_scale,
          "deterministic: lod_scale matches");
}

// =================================================================
//  Disabled response does nothing even on violation
// =================================================================

static void test_disabled_response() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    // Response NOT enabled (default).
    for (int i = 0; i < 10; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget_pressure_level == 0,
          "disabled: pressure stays 0");
    check(!snap.budget_response_active,
          "disabled: response not active");
    check(snap.budget_lod_scale == 1.0f,
          "disabled: lod_scale unchanged");
}

// =================================================================
//  Response config persists across bootstrap
// =================================================================

static void test_config_persists_across_bootstrap() {
    de::SimState sim;

    de::SimBudgetResponseConfig resp{};
    resp.enabled       = true;
    resp.max_pressure  = 3;
    resp.recovery_ticks = 5;
    sim.set_budget_response_config(resp);

    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    // 5 ticks -> pressure should cap at 3 (not default 4).
    for (int i = 0; i < 5; ++i) sim.tick(1.0 / 60.0);

    check(sim.budget_response_state().pressure_level == 3,
          "persist: pressure capped at custom max_pressure=3");
}

// =================================================================
//  Response state resets on bootstrap (no stale pressure)
// =================================================================

static void test_state_resets_on_bootstrap() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    // Build up pressure.
    for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level > 0,
          "state_reset: pressure built up");

    // Re-bootstrap should clear state.
    sim.bootstrap_crowd(cfg);
    check(sim.budget_response_state().pressure_level == 0,
          "state_reset: pressure cleared after bootstrap");
    check(!sim.budget_response_state().active,
          "state_reset: active cleared after bootstrap");
    check(sim.budget_response_state().lod_distance_scale == 1.0f,
          "state_reset: lod_scale reset after bootstrap");
}

// =================================================================
//  LOD thresholds actually change under pressure
// =================================================================

static void test_lod_thresholds_change_under_pressure() {
    // Scene with agents at ~80 units from center -> T2 normally.
    // Under pressure, thresholds shrink -> agents become T3.
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 80.0f;
    cfg.engage_radius   = 1.0f;

    // Run A: no budget response.
    de::SimState sim_a;
    sim_a.bootstrap_crowd(cfg);
    sim_a.tick(1.0 / 60.0);
    auto snap_a = sim_a.snapshot();

    // Agents at dist ~80, default thresholds t2=60 t3=100 -> T2.
    check(snap_a.lod_tier_counts[2] == 10,
          "lod_change: baseline all T2");

    // Run B: with budget response forcing max pressure.
    de::SimState sim_b;
    sim_b.bootstrap_crowd(cfg);
    sim_b.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled          = true;
    resp.max_pressure     = 4;
    resp.shrink_per_level = 0.20f;
    sim_b.set_budget_response_config(resp);

    // 5 ticks: tick 0 builds pressure 1, tick 4 builds pressure 4.
    // Tick 5 would apply pressure 4 but we read snapshot after tick 4.
    // The LOD change from pressure 4 applies at tick 5 start.
    // So we need 5 ticks to build max pressure + 1 more to apply it.
    for (int i = 0; i < 5; ++i) sim_b.tick(1.0 / 60.0);

    // At max pressure, lod_scale=0.2 -> t2_distance=12, t3_distance=20.
    // Agents at dist ~80 > 20 -> should be T3 now.
    // But the scale from tick 4 applies at tick 5 start.
    // After tick 4: pressure=4, scale=0.2 (computed), but LOD applied was
    // from tick 3 (pressure=3, scale=0.4 -> t3=40, dist~80 > 40 -> T3).
    // So agents should already be T3 after a few ticks of high pressure.
    auto snap_b = sim_b.snapshot();
    check(snap_b.lod_tier_counts[3] == 10,
          "lod_change: under pressure all agents pushed to T3");
    check(snap_b.budget_pressure_level == 4,
          "lod_change: pressure at max");
}

// =================================================================
//  Combat core untouched under pressure (still processes all agents)
// =================================================================

static void test_combat_core_untouched() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 1.0f;
    cfg.attack_range    = 10.0f;
    cfg.engage_radius   = 50.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    // Build pressure.
    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();

    // AttackTargets (index 7) must process all living agents.
    // RemoveDead (index 9) processes all agents.
    // These must NOT be zero even under max pressure.
    check(snap.systems[7].entities_processed > 0,
          "combat_core: AttackTargets processes agents under pressure");
    check(snap.systems[9].entities_processed > 0,
          "combat_core: RemoveDead processes agents under pressure");
}

// =================================================================
//  No existing test regressions (sanity: budget + LOD basics)
// =================================================================

static void test_existing_budget_lod_sanity() {
    // Verify that enabling response doesn't break basic budget evaluation.
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    for (int i = 0; i < 10; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget.within_budget,
          "sanity: budget still correctly evaluated");

    // LOD tier sum invariant holds.
    uint32_t tier_sum = 0;
    for (int t = 0; t < 4; ++t) tier_sum += snap.lod_tier_counts[t];
    check(tier_sum == snap.crowd_agent_count,
          "sanity: LOD tier sum == crowd_agent_count");
}

int main() {
    test_under_budget_no_degradation();
    test_over_budget_activates();
    test_pressure_bounded();
    test_progressive_recovery();
    test_deterministic_response();
    test_disabled_response();
    test_config_persists_across_bootstrap();
    test_state_resets_on_bootstrap();
    test_lod_thresholds_change_under_pressure();
    test_combat_core_untouched();
    test_existing_budget_lod_sanity();

    std::printf("\nBudgetResponseTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
