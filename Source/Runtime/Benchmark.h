#pragma once

// Minimal headless benchmark harness for preset stress testing.
//
// Usage:
//   BenchmarkResult r = run_benchmark(k_demo_presets[4], 600);
//   // r contains hash, timing, budget, agent stats.
//
// Comparison helpers:
//   compare_structural(a, b)  -- deterministic signals only (hash,
//       ticks, peak agent/LOD counts).  Stable across machines.
//   compare_budget(a, b)      -- budget signals (ticks_over_budget,
//       pressure).  May vary with wall-clock timing / machine load.

#include "Runtime/DemoPresets.h"
#include "Runtime/SimState.h"

#include <chrono>
#include <cstdint>
#include <cstring>

namespace de {

struct BenchmarkResult {
    char     preset_name[k_system_name_max] = {};
    uint32_t ticks_run          = 0;
    uint64_t final_hash         = 0;

    // -- Deterministic signals (same input -> same output) ------------------
    // Agent metrics (peak over all ticks).
    uint32_t peak_agent_count   = 0;
    uint32_t peak_lod_t0_count  = 0;

    // -- Wall-clock dependent signals (may vary across machines) ------------
    // Timing (wall-clock, seconds).
    double   total_wall_s       = 0.0;
    double   avg_tick_s         = 0.0;
    double   max_tick_s         = 0.0;

    // Budget: number of ticks where at least one budget contract was
    // violated.  Depends on wall-clock timing (max_tick_s, max_system_s
    // contracts compare against real CPU cost).
    uint32_t ticks_over_budget  = 0;
    uint8_t  max_pressure_applied = 0;
    uint8_t  max_pressure_pending = 0;
};

// Run a preset for `tick_count` ticks at `dt` (default 1/60).
// Optionally supply a budget config; pass nullptr to use defaults.
// Optionally supply a budget response config; pass nullptr to skip.
inline BenchmarkResult run_benchmark(
        const DemoPreset& preset,
        uint32_t tick_count,
        double dt = 1.0 / 60.0,
        const SimBudgetConfig* budget_cfg = nullptr,
        const SimBudgetResponseConfig* response_cfg = nullptr) {

    BenchmarkResult r;
    std::snprintf(r.preset_name, k_system_name_max, "%s", preset.name);

    SimState sim;
    apply_demo_preset(sim, preset);

    if (budget_cfg) sim.set_budget_config(*budget_cfg);
    if (response_cfg) sim.set_budget_response_config(*response_cfg);

    auto wall_start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < tick_count; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        sim.tick(dt);
        auto t1 = std::chrono::high_resolution_clock::now();

        double tick_s = std::chrono::duration<double>(t1 - t0).count();
        if (tick_s > r.max_tick_s) r.max_tick_s = tick_s;

        SimSnapshot snap = sim.snapshot();

        if (snap.crowd_agent_count > r.peak_agent_count)
            r.peak_agent_count = snap.crowd_agent_count;
        if (snap.lod_tier_counts[0] > r.peak_lod_t0_count)
            r.peak_lod_t0_count = snap.lod_tier_counts[0];

        if (!snap.budget.within_budget)
            ++r.ticks_over_budget;
        if (snap.budget_response_applied_pressure > r.max_pressure_applied)
            r.max_pressure_applied = snap.budget_response_applied_pressure;
        if (snap.budget_response_pending_pressure > r.max_pressure_pending)
            r.max_pressure_pending = snap.budget_response_pending_pressure;
    }

    auto wall_end = std::chrono::high_resolution_clock::now();
    r.total_wall_s = std::chrono::duration<double>(wall_end - wall_start).count();
    r.ticks_run    = tick_count;
    r.final_hash   = sim.sim_hash();
    r.avg_tick_s   = (tick_count > 0) ? r.total_wall_s / tick_count : 0.0;

    return r;
}

// Compare deterministic signals only.
// Returns true if ticks, hash, and peak agent/LOD counts match.
// Excludes wall-clock timing AND budget violation counts (which
// depend on wall-clock contracts max_tick_s / max_system_s).
// Safe to use across machines and under varying load.
inline bool compare_structural(const BenchmarkResult& a,
                               const BenchmarkResult& b) {
    if (a.ticks_run != b.ticks_run) return false;
    if (a.final_hash != b.final_hash) return false;
    if (a.peak_agent_count != b.peak_agent_count) return false;
    if (a.peak_lod_t0_count != b.peak_lod_t0_count) return false;
    return true;
}

// Compare budget signals (wall-clock dependent).
// Returns true if ticks_over_budget and pressure peaks match.
// Only meaningful when comparing runs on the same machine under
// similar load conditions.
inline bool compare_budget(const BenchmarkResult& a,
                           const BenchmarkResult& b) {
    if (a.ticks_over_budget != b.ticks_over_budget) return false;
    if (a.max_pressure_applied != b.max_pressure_applied) return false;
    if (a.max_pressure_pending != b.max_pressure_pending) return false;
    return true;
}

}  // namespace de
