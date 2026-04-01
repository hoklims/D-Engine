#include "Runtime/Benchmark.h"
#include "Runtime/DemoPresets.h"

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
//  Benchmark harness is deterministic (same preset -> same hash)
// =================================================================

static void test_benchmark_determinism() {
    auto r1 = de::run_benchmark(de::k_demo_presets[0], 120);
    auto r2 = de::run_benchmark(de::k_demo_presets[0], 120);

    check(de::compare_structural(r1, r2),
          "determinism: two runs of LaneClash produce identical results");
    check(r1.final_hash != 0,
          "determinism: hash is non-zero");
    check(r1.ticks_run == 120,
          "determinism: correct tick count");
}

// =================================================================
//  Stress presets exist and are heavier than demo presets
// =================================================================

static void test_stress_presets_heavier() {
    // Demo preset 0 (LaneClash) = 40 agents/team = 80 total.
    auto demo = de::run_benchmark(de::k_demo_presets[0], 1);

    // Stress preset 4 (StressLane) = 200 agents/team = 400 total.
    check(de::k_demo_preset_count >= 7,
          "stress_presets: at least 7 presets (4 demo + 3 stress)");

    auto stress_lane  = de::run_benchmark(de::k_demo_presets[4], 1);
    auto stress_dense = de::run_benchmark(de::k_demo_presets[5], 1);
    auto stress_wall  = de::run_benchmark(de::k_demo_presets[6], 1);

    check(stress_lane.peak_agent_count > demo.peak_agent_count,
          "stress_presets: StressLane has more agents than LaneClash");
    check(stress_dense.peak_agent_count > demo.peak_agent_count,
          "stress_presets: StressDenseMelee has more agents than LaneClash");
    check(stress_wall.peak_agent_count > demo.peak_agent_count,
          "stress_presets: StressWallGap has more agents than LaneClash");

    // Verify names.
    check(std::strcmp(stress_lane.preset_name, "StressLane") == 0,
          "stress_presets: StressLane name correct");
    check(std::strcmp(stress_dense.preset_name, "StressDenseMelee") == 0,
          "stress_presets: StressDenseMelee name correct");
    check(std::strcmp(stress_wall.preset_name, "StressWallGap") == 0,
          "stress_presets: StressWallGap name correct");
}

// =================================================================
//  Stress presets are deterministic
// =================================================================

static void test_stress_determinism() {
    for (int i = 4; i < 7; ++i) {
        auto r1 = de::run_benchmark(de::k_demo_presets[i], 60);
        auto r2 = de::run_benchmark(de::k_demo_presets[i], 60);
        check(de::compare_structural(r1, r2),
              "stress_determinism: identical runs match");
    }
}

// =================================================================
//  Budget violations observable under tight budget
// =================================================================

static void test_budget_observable() {
    de::SimBudgetConfig tight;
    tight.max_tick_s            = 0.0;     // impossible to satisfy
    tight.max_system_s          = 0.0;
    tight.max_targeting_scanned = 0;
    tight.max_melee_checks      = 0;
    tight.max_lod_t0_count      = 0;

    auto r = de::run_benchmark(de::k_demo_presets[4], 10,
                               1.0 / 60.0, &tight);

    check(r.ticks_over_budget > 0,
          "budget_observable: violations detected under tight budget");
    check(r.ticks_over_budget == r.ticks_run,
          "budget_observable: every tick violated (impossible budget)");
}

// =================================================================
//  Budget response pressure observable under tight budget
// =================================================================

static void test_budget_response_observable() {
    de::SimBudgetConfig tight;
    tight.max_tick_s            = 0.0;
    tight.max_system_s          = 0.0;
    tight.max_targeting_scanned = 0;
    tight.max_melee_checks      = 0;
    tight.max_lod_t0_count      = 0;

    de::SimBudgetResponseConfig resp;
    resp.enabled       = true;
    resp.max_pressure  = 4;
    resp.shrink_per_level = 0.20f;
    resp.recovery_ticks   = 3;

    auto r = de::run_benchmark(de::k_demo_presets[4], 20,
                               1.0 / 60.0, &tight, &resp);

    check(r.max_pressure_pending > 0,
          "budget_response: pending pressure observed");
    check(r.max_pressure_applied > 0,
          "budget_response: applied pressure observed");
}

// =================================================================
//  compare_structural detects mismatches
// =================================================================

static void test_compare_detects_mismatch() {
    auto r1 = de::run_benchmark(de::k_demo_presets[0], 60);
    auto r2 = de::run_benchmark(de::k_demo_presets[1], 60);

    check(!de::compare_structural(r1, r2),
          "compare: different presets produce different results");
}

// =================================================================
//  Structural comparison ignores budget/wall-clock signals
// =================================================================
// Two runs of the same preset with different budget configs must
// still compare as structurally equal.  The budget fields should
// differ, but compare_structural must not care.

static void test_structural_ignores_budget() {
    // Run 1: default budget (very generous, 0 violations expected).
    auto r1 = de::run_benchmark(de::k_demo_presets[0], 60);

    // Run 2: impossible budget (every tick violates).
    de::SimBudgetConfig tight;
    tight.max_tick_s            = 0.0;
    tight.max_system_s          = 0.0;
    tight.max_targeting_scanned = 0;
    tight.max_melee_checks      = 0;
    tight.max_lod_t0_count      = 0;
    auto r2 = de::run_benchmark(de::k_demo_presets[0], 60,
                                1.0 / 60.0, &tight);

    // Budget fields differ.
    check(r1.ticks_over_budget != r2.ticks_over_budget,
          "structural_ignores_budget: budget fields actually differ");

    // Structural comparison still passes (same sim, same hash).
    check(de::compare_structural(r1, r2),
          "structural_ignores_budget: compare_structural is stable");

    // Budget comparison detects the difference.
    check(!de::compare_budget(r1, r2),
          "structural_ignores_budget: compare_budget detects diff");
}

// =================================================================
//  compare_budget works for identical runs
// =================================================================

static void test_compare_budget_identical() {
    de::SimBudgetConfig tight;
    tight.max_tick_s            = 0.0;
    tight.max_system_s          = 0.0;
    tight.max_targeting_scanned = 0;
    tight.max_melee_checks      = 0;
    tight.max_lod_t0_count      = 0;

    auto r1 = de::run_benchmark(de::k_demo_presets[0], 30,
                                1.0 / 60.0, &tight);
    auto r2 = de::run_benchmark(de::k_demo_presets[0], 30,
                                1.0 / 60.0, &tight);

    // With deterministic-only budget contracts (targeting, melee, lod),
    // the count-based violations are identical across runs.
    // Wall-clock contracts (max_tick_s=0) always fire, so both runs
    // see the same violation count.
    check(r1.ticks_over_budget == r2.ticks_over_budget,
          "compare_budget_identical: same violation count");
    check(de::compare_budget(r1, r2),
          "compare_budget_identical: budget signals match");
}

// =================================================================
//  Benchmark result fields are populated
// =================================================================

static void test_result_fields() {
    // Use StressDenseMelee (preset 5): tight spawn, agents near battle
    // center from tick 1, so lod T0 is populated immediately.
    auto r = de::run_benchmark(de::k_demo_presets[5], 30);

    check(r.ticks_run == 30,        "fields: ticks_run");
    check(r.final_hash != 0,        "fields: final_hash non-zero");
    check(r.total_wall_s > 0.0,     "fields: total_wall_s > 0");
    check(r.avg_tick_s > 0.0,       "fields: avg_tick_s > 0");
    check(r.max_tick_s > 0.0,       "fields: max_tick_s > 0");
    check(r.peak_agent_count > 0,   "fields: peak_agent_count > 0");
    check(r.peak_lod_t0_count > 0,  "fields: peak_lod_t0_count > 0");
}

// =================================================================

int main() {
    test_benchmark_determinism();
    test_stress_presets_heavier();
    test_stress_determinism();
    test_budget_observable();
    test_budget_response_observable();
    test_compare_detects_mismatch();
    test_structural_ignores_budget();
    test_compare_budget_identical();
    test_result_fields();

    std::printf("\nBenchmarkTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
