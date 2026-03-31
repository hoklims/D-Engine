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

static de::SimBudgetConfig generous_budget() {
    de::SimBudgetConfig b{};
    b.max_tick_s            = 100.0;
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

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    for (int i = 0; i < 20; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget_response_applied_pressure == 0,
          "under_budget: applied pressure stays 0");
    check(!snap.budget_response_applied_active,
          "under_budget: applied not active");
    check(snap.budget_response_applied_lod_scale == 1.0f,
          "under_budget: applied lod_scale remains 1.0");
    check(snap.budget_response_pending_pressure == 0,
          "under_budget: pending pressure stays 0");
    check(snap.budget_response_pending_lod_scale == 1.0f,
          "under_budget: pending lod_scale remains 1.0");
}

// =================================================================
//  Over budget: degradation activates (pending, not yet applied)
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

    // Pending state: decision made for next tick.
    check(snap.budget_response_pending_pressure == 1,
          "over_budget: pending pressure increased to 1");
    check(snap.budget_response_pending_lod_scale < 1.0f,
          "over_budget: pending lod_scale reduced");
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

    for (int i = 0; i < 10; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget_response_pending_pressure == 4,
          "bounded: pending pressure capped at max_pressure=4");
    check(snap.budget_response_pending_lod_scale > 0.0f,
          "bounded: pending lod_scale stays positive");
    check(snap.budget_response_pending_lod_scale <= 0.25f,
          "bounded: pending lod_scale at max pressure ~0.2");
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

    // 4 ticks of violation -> pending pressure = 4.
    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 4,
          "recovery: pending pressure reached 4");

    // Relax budget.
    sim.set_budget_config(generous_budget());

    // 1 tick: still at pressure 4 (need 3 healthy ticks for -1).
    sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 4,
          "recovery: pending pressure still 4 after 1 healthy tick");

    // 2 more ticks (total 3 healthy) -> pending pressure drops to 3.
    sim.tick(1.0 / 60.0);
    sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 3,
          "recovery: pending pressure dropped to 3 after 3 healthy ticks");

    // 3 more healthy ticks -> pending pressure drops to 2.
    for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 2,
          "recovery: pending pressure dropped to 2");

    // Full recovery: 6 more healthy ticks -> pending pressure 0.
    for (int i = 0; i < 6; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 0,
          "recovery: pending pressure back to 0");
    check(!sim.budget_response_state().active,
          "recovery: response deactivated");
    check(sim.budget_response_state().lod_distance_scale == 1.0f,
          "recovery: pending lod_scale back to 1.0");
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

        for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);
        sim.set_budget_config(generous_budget());
        for (int i = 0; i < 5; ++i) sim.tick(1.0 / 60.0);

        return sim.snapshot();
    };

    de::SimState sim1, sim2;
    auto snap1 = run(sim1);
    auto snap2 = run(sim2);

    check(snap1.budget_response_pending_pressure == snap2.budget_response_pending_pressure,
          "deterministic: pending_pressure matches");
    check(snap1.budget_response_applied_pressure == snap2.budget_response_applied_pressure,
          "deterministic: applied_pressure matches");
    check(snap1.budget_response_pending_lod_scale == snap2.budget_response_pending_lod_scale,
          "deterministic: pending_lod_scale matches");
    check(snap1.budget_response_applied_lod_scale == snap2.budget_response_applied_lod_scale,
          "deterministic: applied_lod_scale matches");
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

    for (int i = 0; i < 10; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget_response_pending_pressure == 0,
          "disabled: pending pressure stays 0");
    check(!snap.budget_response_applied_active,
          "disabled: applied not active");
    check(snap.budget_response_applied_lod_scale == 1.0f,
          "disabled: applied lod_scale unchanged");
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

    for (int i = 0; i < 5; ++i) sim.tick(1.0 / 60.0);

    check(sim.budget_response_state().pressure_level == 3,
          "persist: pending pressure capped at custom max_pressure=3");
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

    for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level > 0,
          "state_reset: pressure built up");

    sim.bootstrap_crowd(cfg);
    check(sim.budget_response_state().pressure_level == 0,
          "state_reset: pending pressure cleared after bootstrap");
    check(!sim.budget_response_state().active,
          "state_reset: pending active cleared after bootstrap");
    check(sim.budget_response_state().lod_distance_scale == 1.0f,
          "state_reset: pending lod_scale reset after bootstrap");

    // Applied state also clean after bootstrap.
    auto snap = sim.snapshot();
    check(snap.budget_response_applied_pressure == 0,
          "state_reset: applied pressure cleared after bootstrap");
    check(!snap.budget_response_applied_active,
          "state_reset: applied active cleared after bootstrap");
}

// =================================================================
//  LOD thresholds actually change under pressure
// =================================================================

static void test_lod_thresholds_change_under_pressure() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 80.0f;
    cfg.engage_radius   = 1.0f;

    // Run A: no budget response.
    de::SimState sim_a;
    sim_a.bootstrap_crowd(cfg);
    sim_a.tick(1.0 / 60.0);
    auto snap_a = sim_a.snapshot();
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

    for (int i = 0; i < 5; ++i) sim_b.tick(1.0 / 60.0);

    auto snap_b = sim_b.snapshot();
    check(snap_b.lod_tier_counts[3] == 10,
          "lod_change: under pressure all agents pushed to T3");
    check(snap_b.budget_response_pending_pressure == 4,
          "lod_change: pending pressure at max");
}

// =================================================================
//  Combat core untouched under pressure
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

    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.systems[7].entities_processed > 0,
          "combat_core: AttackTargets processes agents under pressure");
    check(snap.systems[9].entities_processed > 0,
          "combat_core: RemoveDead processes agents under pressure");
}

// =================================================================
//  Sanity: budget + LOD basics still work with response enabled
// =================================================================

static void test_existing_budget_lod_sanity() {
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

    uint32_t tier_sum = 0;
    for (int t = 0; t < 4; ++t) tier_sum += snap.lod_tier_counts[t];
    check(tier_sum == snap.crowd_agent_count,
          "sanity: LOD tier sum == crowd_agent_count");
}

// =================================================================
//  NEW: First violation tick -- applied is clean, pending shows pressure
// =================================================================

static void test_applied_vs_pending_first_violation() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    // First tick: violation detected, response decided for next tick.
    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    // Applied: this tick ran WITHOUT degradation (no prior decision).
    check(snap.budget_response_applied_pressure == 0,
          "first_viol: applied pressure is 0 (no degradation this tick)");
    check(!snap.budget_response_applied_active,
          "first_viol: applied not active (ran clean)");
    check(snap.budget_response_applied_lod_scale == 1.0f,
          "first_viol: applied lod_scale is 1.0 (no shrink)");

    // Pending: decision made for next tick.
    check(snap.budget_response_pending_pressure == 1,
          "first_viol: pending pressure is 1 (decided for next tick)");
    check(snap.budget_response_pending_lod_scale < 1.0f,
          "first_viol: pending lod_scale < 1.0 (will shrink next tick)");
}

// =================================================================
//  NEW: Second tick -- applied reflects previous decision
// =================================================================

static void test_applied_coherent_second_tick() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled          = true;
    resp.shrink_per_level = 0.20f;
    sim.set_budget_response_config(resp);

    // Tick 0: decides pressure=1 for tick 1.
    sim.tick(1.0 / 60.0);

    // Tick 1: applies pressure=1, decides pressure=2 for tick 2.
    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    // Applied: pressure 1 was used this tick.
    check(snap.budget_response_applied_pressure == 1,
          "second_tick: applied pressure is 1");
    check(snap.budget_response_applied_active,
          "second_tick: applied is active");
    check(snap.budget_response_applied_lod_scale == 0.8f,
          "second_tick: applied lod_scale is 0.8 (1 - 1*0.2)");

    // Pending: pressure 2 decided for tick 2.
    check(snap.budget_response_pending_pressure == 2,
          "second_tick: pending pressure is 2");
    check(snap.budget_response_pending_lod_scale == 0.6f,
          "second_tick: pending lod_scale is 0.6 (1 - 2*0.2)");
}

// =================================================================
//  NEW: Hot-disable clears public state immediately
// =================================================================

static void test_hot_disable_clears_state() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    // Build up pressure.
    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 4,
          "hot_disable: pressure built up to 4");

    // Disable the response mid-run.
    resp.enabled = false;
    sim.set_budget_response_config(resp);

    // Next tick: disabled response resets state.
    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    // Applied: disabled -> nominal.
    check(snap.budget_response_applied_pressure == 0,
          "hot_disable: applied pressure is 0 after disable");
    check(!snap.budget_response_applied_active,
          "hot_disable: applied not active after disable");
    check(snap.budget_response_applied_lod_scale == 1.0f,
          "hot_disable: applied lod_scale is 1.0 after disable");

    // Pending: state fully cleaned by apply_budget_response().
    check(snap.budget_response_pending_pressure == 0,
          "hot_disable: pending pressure is 0 after disable");
    check(snap.budget_response_pending_lod_scale == 1.0f,
          "hot_disable: pending lod_scale is 1.0 after disable");

    // Raw accessor also clean.
    check(sim.budget_response_state().pressure_level == 0,
          "hot_disable: raw state pressure is 0");
    check(!sim.budget_response_state().active,
          "hot_disable: raw state not active");
}

// =================================================================
//  NEW: Hot-disable via max_pressure=0 also clears state
// =================================================================

static void test_hot_disable_max_pressure_zero() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 3,
          "disable_maxp0: pressure built up to 3");

    // Disable via max_pressure=0 (enabled stays true).
    resp.max_pressure = 0;
    sim.set_budget_response_config(resp);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(snap.budget_response_applied_pressure == 0,
          "disable_maxp0: applied pressure is 0");
    check(!snap.budget_response_applied_active,
          "disable_maxp0: applied not active");
    check(snap.budget_response_pending_pressure == 0,
          "disable_maxp0: pending pressure is 0");
    check(sim.budget_response_state().pressure_level == 0,
          "disable_maxp0: raw state pressure is 0");
}

// =================================================================
//  Hot-disable without tick: state clean immediately
// =================================================================

static void test_hot_disable_immediate_no_tick() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    // Build pressure.
    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 4,
          "imm_disable: pressure built up to 4");

    // Disable -- no tick after this.
    resp.enabled = false;
    sim.set_budget_response_config(resp);

    // Raw accessor must be clean immediately.
    check(sim.budget_response_state().pressure_level == 0,
          "imm_disable: raw pressure is 0 immediately");
    check(!sim.budget_response_state().active,
          "imm_disable: raw active is false immediately");
    check(sim.budget_response_state().lod_distance_scale == 1.0f,
          "imm_disable: raw lod_scale is 1.0 immediately");

    // Snapshot must also be clean (pending fields).
    auto snap = sim.snapshot();
    check(snap.budget_response_pending_pressure == 0,
          "imm_disable: snap pending pressure is 0 immediately");
    check(snap.budget_response_pending_lod_scale == 1.0f,
          "imm_disable: snap pending lod_scale is 1.0 immediately");
}

// =================================================================
//  Hot-lower max_pressure clamps pressure immediately
// =================================================================

static void test_hot_lower_max_pressure_clamps() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled          = true;
    resp.max_pressure     = 4;
    resp.shrink_per_level = 0.20f;
    sim.set_budget_response_config(resp);

    // Build pressure to 4.
    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 4,
          "hot_lower: pressure built up to 4");

    // Lower max_pressure to 2 -- no tick after this.
    resp.max_pressure = 2;
    sim.set_budget_response_config(resp);

    // Pressure must be clamped to 2 immediately.
    check(sim.budget_response_state().pressure_level == 2,
          "hot_lower: pressure clamped to 2 immediately");
    check(sim.budget_response_state().active,
          "hot_lower: still active at pressure 2");

    // Scale must be recomputed: 1.0 - 2*0.20 = 0.60.
    check(sim.budget_response_state().lod_distance_scale == 0.6f,
          "hot_lower: lod_scale recomputed to 0.6 immediately");

    // Snapshot coherent without tick.
    auto snap = sim.snapshot();
    check(snap.budget_response_pending_pressure == 2,
          "hot_lower: snap pending pressure is 2");
    check(snap.budget_response_pending_lod_scale == 0.6f,
          "hot_lower: snap pending lod_scale is 0.6");
}

// =================================================================
//  Hot-lower max_pressure to 0 is full reset
// =================================================================

static void test_hot_lower_max_pressure_to_zero() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled = true;
    sim.set_budget_response_config(resp);

    for (int i = 0; i < 3; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 3,
          "lower_to_0: pressure built up to 3");

    // Lower to 0 -- no tick after.
    resp.max_pressure = 0;
    sim.set_budget_response_config(resp);

    check(sim.budget_response_state().pressure_level == 0,
          "lower_to_0: pressure reset to 0 immediately");
    check(!sim.budget_response_state().active,
          "lower_to_0: not active immediately");
    check(sim.budget_response_state().lod_distance_scale == 1.0f,
          "lower_to_0: lod_scale is 1.0 immediately");
}

// =================================================================
//  Hot-raise max_pressure does not change current pressure
// =================================================================

static void test_hot_raise_max_pressure_no_change() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled          = true;
    resp.max_pressure     = 2;
    resp.shrink_per_level = 0.20f;
    sim.set_budget_response_config(resp);

    // Build pressure to cap at 2.
    for (int i = 0; i < 5; ++i) sim.tick(1.0 / 60.0);
    check(sim.budget_response_state().pressure_level == 2,
          "hot_raise: pressure capped at 2");

    // Raise max_pressure to 6 -- pressure must stay at 2 (no inflation).
    resp.max_pressure = 6;
    sim.set_budget_response_config(resp);

    check(sim.budget_response_state().pressure_level == 2,
          "hot_raise: pressure stays at 2 after raising cap");
    check(sim.budget_response_state().lod_distance_scale == 0.6f,
          "hot_raise: lod_scale unchanged at 0.6");
}

// =================================================================
//  Snapshot after hot-lower then tick: applied reflects clamped value
// =================================================================

static void test_hot_lower_then_tick_applied_coherent() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_budget_config(always_over_budget());

    de::SimBudgetResponseConfig resp{};
    resp.enabled          = true;
    resp.max_pressure     = 4;
    resp.shrink_per_level = 0.20f;
    sim.set_budget_response_config(resp);

    // Build pressure to 4.
    for (int i = 0; i < 4; ++i) sim.tick(1.0 / 60.0);

    // Lower max to 1 -- clamps to 1, scale = 0.8.
    resp.max_pressure = 1;
    sim.set_budget_response_config(resp);
    check(sim.budget_response_state().pressure_level == 1,
          "lower_tick: clamped to 1 before tick");

    // Next tick: applied should reflect pressure 1.
    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(snap.budget_response_applied_pressure == 1,
          "lower_tick: applied pressure is 1");
    check(snap.budget_response_applied_active,
          "lower_tick: applied is active");
    check(snap.budget_response_applied_lod_scale == 0.8f,
          "lower_tick: applied lod_scale is 0.8 (1 - 1*0.2)");

    // Pending stays at 1 (already capped, budget still over).
    check(snap.budget_response_pending_pressure == 1,
          "lower_tick: pending pressure stays at 1 (capped)");
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
    test_applied_vs_pending_first_violation();
    test_applied_coherent_second_tick();
    test_hot_disable_clears_state();
    test_hot_disable_max_pressure_zero();
    test_hot_disable_immediate_no_tick();
    test_hot_lower_max_pressure_clamps();
    test_hot_lower_max_pressure_to_zero();
    test_hot_raise_max_pressure_no_change();
    test_hot_lower_then_tick_applied_coherent();

    std::printf("\nBudgetResponseTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
