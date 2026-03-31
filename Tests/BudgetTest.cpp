#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"

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
//  Normal scene stays within default budget
// =================================================================

static void test_normal_within_budget() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Default budget is generous -- 10 agents should never exceed it.
    for (int i = 0; i < 10; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.budget.within_budget, "normal: within_budget");
    check(snap.budget.violation_count == 0, "normal: zero violations");
    check(!snap.budget.tick_over, "normal: tick not over");
    check(!snap.budget.system_over, "normal: system not over");
    check(!snap.budget.targeting_over, "normal: targeting not over");
    check(!snap.budget.melee_over, "normal: melee not over");
    check(!snap.budget.lod_t0_over, "normal: lod_t0 not over");
}

// =================================================================
//  Artificially tight targeting budget triggers violation
// =================================================================

static void test_targeting_violation() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Set targeting budget impossibly low.
    de::SimBudgetConfig budget{};
    budget.max_targeting_scanned = 1;
    sim.set_budget_config(budget);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(!snap.budget.within_budget, "targeting_viol: not within budget");
    check(snap.budget.targeting_over, "targeting_viol: targeting_over flag");
    check(snap.budget.targeting_scanned > 1,
          "targeting_viol: scanned exceeds threshold");
    check(snap.budget.violation_count >= 1,
          "targeting_viol: violation_count >= 1");
}

// =================================================================
//  Artificially tight melee budget triggers violation
// =================================================================

static void test_melee_violation() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;
    cfg.team_spacing    = 1.0f;   // close enough for melee
    cfg.attack_range    = 10.0f;  // wide range to guarantee broadphase work

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    de::SimBudgetConfig budget{};
    budget.max_melee_checks = 1;
    sim.set_budget_config(budget);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(!snap.budget.within_budget, "melee_viol: not within budget");
    check(snap.budget.melee_over, "melee_viol: melee_over flag");
    check(snap.budget.melee_checks > 1,
          "melee_viol: checks exceed threshold");
}

// =================================================================
//  LOD T0 budget violation with many agents forced to T0
// =================================================================

static void test_lod_t0_violation() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 20;
    cfg.team_spacing    = 1.0f;   // very close -- most agents near center
    cfg.engage_radius   = 50.0f;  // wide engage -> all T0

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    // Center LOD at the agents so they all classify as T0.
    sim.set_lod_center(0.0f, 0.0f);

    de::SimBudgetConfig budget{};
    budget.max_lod_t0_count = 5;  // impossibly low
    sim.set_budget_config(budget);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(!snap.budget.within_budget, "lod_t0_viol: not within budget");
    check(snap.budget.lod_t0_over, "lod_t0_viol: lod_t0_over flag");
    check(snap.budget.lod_t0_count > 5,
          "lod_t0_viol: t0 count exceeds threshold");
}

// =================================================================
//  Hottest system is identified correctly
// =================================================================

static void test_hottest_system() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();

    // Hottest system name must be non-empty.
    check(snap.budget.hottest_system[0] != '\0',
          "hottest: name is non-empty");
    // Hottest elapsed must be >= 0.
    check(snap.budget.hottest_system_s >= 0.0,
          "hottest: elapsed >= 0");
    // Hottest elapsed must be <= total tick elapsed.
    check(snap.budget.hottest_system_s <= snap.budget.tick_elapsed_s,
          "hottest: elapsed <= tick total");

    // Verify hottest matches one of the system names in snapshot.
    bool found = false;
    for (uint32_t i = 0; i < snap.system_count; ++i) {
        if (std::strcmp(snap.systems[i].name, snap.budget.hottest_system) == 0) {
            found = true;
            break;
        }
    }
    check(found, "hottest: name matches a registered system");
}

// =================================================================
//  Budget snapshot is coherent with telemetry snapshot
// =================================================================

static void test_snapshot_coherence() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;
    cfg.team_spacing    = 2.0f;
    cfg.attack_range    = 5.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    for (int i = 0; i < 5; ++i) sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();

    // Budget targeting_scanned must match snapshot targeting_candidates_scanned.
    check(snap.budget.targeting_scanned == snap.targeting_candidates_scanned,
          "coherence: targeting_scanned matches");
    // Budget melee_checks must match snapshot melee_broadphase_checks.
    check(snap.budget.melee_checks == snap.melee_broadphase_checks,
          "coherence: melee_checks matches");
    // Budget lod_t0_count must match snapshot lod_tier_counts[0].
    check(snap.budget.lod_t0_count == snap.lod_tier_counts[0],
          "coherence: lod_t0_count matches");
    // Tick elapsed must be positive (systems ran).
    check(snap.budget.tick_elapsed_s > 0.0,
          "coherence: tick_elapsed_s > 0");
}

// =================================================================
//  Multiple violations accumulate correctly
// =================================================================

static void test_multiple_violations() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 10;
    cfg.team_spacing    = 1.0f;
    cfg.attack_range    = 10.0f;
    cfg.engage_radius   = 50.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.set_lod_center(0.0f, 0.0f);

    // Set all work budgets impossibly low.
    de::SimBudgetConfig budget{};
    budget.max_targeting_scanned = 1;
    budget.max_melee_checks      = 1;
    budget.max_lod_t0_count      = 1;
    // Keep tick/system budgets generous so only work contracts fire.
    budget.max_tick_s            = 10.0;
    budget.max_system_s          = 10.0;
    sim.set_budget_config(budget);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(!snap.budget.within_budget, "multi_viol: not within budget");
    check(snap.budget.targeting_over, "multi_viol: targeting_over");
    check(snap.budget.melee_over, "multi_viol: melee_over");
    check(snap.budget.lod_t0_over, "multi_viol: lod_t0_over");
    check(!snap.budget.tick_over, "multi_viol: tick not over (generous)");
    check(!snap.budget.system_over, "multi_viol: system not over (generous)");
    check(snap.budget.violation_count == 3,
          "multi_viol: exactly 3 violations");
}

// =================================================================
//  Budget status resets between ticks (no stale flags)
// =================================================================

static void test_budget_resets_between_ticks() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // First tick: force a violation.
    de::SimBudgetConfig tight{};
    tight.max_targeting_scanned = 1;
    sim.set_budget_config(tight);
    sim.tick(1.0 / 60.0);
    auto snap1 = sim.snapshot();
    check(snap1.budget.targeting_over, "reset: tick1 has violation");

    // Second tick: relax budget -- violation should clear.
    de::SimBudgetConfig generous{};
    generous.max_targeting_scanned = 999999;
    sim.set_budget_config(generous);
    sim.tick(1.0 / 60.0);
    auto snap2 = sim.snapshot();
    check(!snap2.budget.targeting_over, "reset: tick2 targeting cleared");
    check(snap2.budget.within_budget, "reset: tick2 within budget");
    check(snap2.budget.violation_count == 0, "reset: tick2 zero violations");
}

// =================================================================
//  budget_status() accessor matches snapshot
// =================================================================

static void test_accessor_matches_snapshot() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0 / 60.0);

    const auto& status = sim.budget_status();
    auto snap = sim.snapshot();

    check(status.within_budget == snap.budget.within_budget,
          "accessor: within_budget matches");
    check(status.violation_count == snap.budget.violation_count,
          "accessor: violation_count matches");
    check(status.tick_elapsed_s == snap.budget.tick_elapsed_s,
          "accessor: tick_elapsed_s matches");
    check(std::strcmp(status.hottest_system, snap.budget.hottest_system) == 0,
          "accessor: hottest_system matches");
}

int main() {
    test_normal_within_budget();
    test_targeting_violation();
    test_melee_violation();
    test_lod_t0_violation();
    test_hottest_system();
    test_snapshot_coherence();
    test_multiple_violations();
    test_budget_resets_between_ticks();
    test_accessor_matches_snapshot();

    std::printf("\nBudgetTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
