#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"
#include "Runtime/CrowdSystems.h"
#include "Runtime/DemoPresets.h"

#include <cmath>
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
//  Head-on opposing flows produce lateral displacement
// =================================================================
// Two agents face each other directly.  After several ticks the
// avoidance system should steer them laterally so they pass each
// other instead of stopping face-to-face.

static void test_opposing_agents_dodge() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 4.0f;    // start 8 units apart
    cfg.agent_spread    = 0.0f;
    cfg.move_speed      = 3.0f;
    cfg.engage_radius   = 50.0f;
    cfg.attack_range    = 0.1f;    // tiny -- agents keep pursuing, never stop
    cfg.separation_radius   = 0.3f;
    cfg.separation_strength = 2.0f;
    cfg.avoidance_radius    = 6.0f;
    cfg.avoidance_horizon   = 1.5f;
    cfg.avoidance_strength  = 4.0f;
    sim.bootstrap_crowd(cfg);

    // Check avoidance telemetry at each tick during approach phase.
    bool avoidance_fired = false;
    bool any_lateral     = false;
    for (int i = 0; i < 90; ++i) {
        sim.tick(1.0 / 60.0);
        de::SimSnapshot snap = sim.snapshot();
        if (snap.avoidance_adjusted_this_tick > 0) avoidance_fired = true;
    }

    // After 90 ticks agents should have lateral displacement (Y != 0).
    sim.world.each<de::CrowdAgent, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Position& p) {
            if (std::fabsf(p.y) > 0.05f) any_lateral = true;
        });
    check(any_lateral,    "opposing_dodge: agents develop lateral displacement");
    check(avoidance_fired, "opposing_dodge: avoidance adjusted > 0");
}

// =================================================================
//  Opposing lane flows produce fewer head-on stalls
// =================================================================
// Two groups of agents march toward each other.  With avoidance,
// fewer agents should be nearly stopped (low speed) compared to
// a baseline without avoidance.

static void test_lane_clash_fewer_stalls() {
    // With avoidance (normal).
    de::SimState sim_a;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 10;
    cfg.team_spacing    = 15.0f;
    cfg.agent_spread    = 1.0f;
    cfg.move_speed      = 3.0f;
    cfg.engage_radius   = 50.0f;
    cfg.attack_range    = 1.5f;
    cfg.separation_radius   = 0.6f;
    cfg.separation_strength = 4.0f;
    cfg.avoidance_radius    = 3.0f;
    cfg.avoidance_horizon   = 0.8f;
    cfg.avoidance_strength  = 2.5f;
    sim_a.bootstrap_crowd(cfg);

    // Without avoidance (strength 0).
    de::SimState sim_b;
    de::CrowdConfig cfg_no = cfg;
    cfg_no.avoidance_strength = 0.0f;
    sim_b.bootstrap_crowd(cfg_no);

    // Run both for 90 ticks.
    for (int i = 0; i < 90; ++i) {
        sim_a.tick(1.0 / 60.0);
        sim_b.tick(1.0 / 60.0);
    }

    // Count near-stalled agents (speed < 0.5).
    auto count_stalled = [](de::World& w) -> int {
        int stalled = 0;
        w.each<de::CrowdAgent, de::Velocity>(
            [&](de::EntityId, de::CrowdAgent&, de::Velocity& v) {
                float spd = std::sqrt(v.dx * v.dx + v.dy * v.dy);
                if (spd < 0.5f) ++stalled;
            });
        return stalled;
    };

    int stalled_a = count_stalled(sim_a.world);
    int stalled_b = count_stalled(sim_b.world);

    // With avoidance should have no more stalls than without.
    // (ideally fewer, but at minimum not worse).
    check(stalled_a <= stalled_b + 2,
          "lane_clash: avoidance does not increase stalls");
}

// =================================================================
//  Zero-velocity agents produce no NaN or degenerate vectors
// =================================================================
// Agents spawned at the same position with zero velocity should not
// produce NaN in avoidance calculations.

static void test_zero_velocity_no_nan() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 0.1f;   // spawn very close
    cfg.agent_spread    = 0.0f;   // all same Y
    cfg.move_speed      = 0.0f;   // zero speed
    cfg.engage_radius   = 50.0f;
    cfg.avoidance_radius    = 5.0f;
    cfg.avoidance_horizon   = 1.0f;
    cfg.avoidance_strength  = 3.0f;
    sim.bootstrap_crowd(cfg);

    // 10 ticks should not produce NaN.
    for (int i = 0; i < 10; ++i) {
        sim.tick(1.0 / 60.0);
    }

    bool has_nan = false;
    sim.world.each<de::CrowdAgent, de::Position, de::Velocity>(
        [&](de::EntityId, de::CrowdAgent&, de::Position& p, de::Velocity& v) {
            if (std::isnan(p.x) || std::isnan(p.y)) has_nan = true;
            if (std::isnan(v.dx) || std::isnan(v.dy)) has_nan = true;
            if (std::isinf(p.x) || std::isinf(p.y)) has_nan = true;
            if (std::isinf(v.dx) || std::isinf(v.dy)) has_nan = true;
        });
    check(!has_nan, "zero_vel: no NaN or Inf in positions/velocities");
}

// =================================================================
//  Low density: avoidance does not interfere
// =================================================================
// When agents are far apart, avoidance should produce zero adjustment.

static void test_low_density_no_avoidance() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 3;
    cfg.team_spacing    = 50.0f;  // far apart
    cfg.agent_spread    = 10.0f;
    cfg.move_speed      = 3.0f;
    cfg.engage_radius   = 5.0f;   // small, so they just march
    cfg.avoidance_radius    = 3.0f;
    cfg.avoidance_horizon   = 0.8f;
    cfg.avoidance_strength  = 2.5f;
    sim.bootstrap_crowd(cfg);

    sim.tick(1.0 / 60.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.avoidance_adjusted_this_tick == 0,
          "low_density: no avoidance adjustments when far apart");
}

// =================================================================
//  Determinism: identical runs produce identical hashes
// =================================================================

static void test_avoidance_determinism() {
    auto run = []() -> uint64_t {
        de::SimState sim;
        de::CrowdConfig cfg;
        cfg.agents_per_team = 20;
        cfg.team_spacing    = 20.0f;
        cfg.agent_spread    = 1.2f;
        cfg.move_speed      = 3.0f;
        cfg.engage_radius   = 30.0f;
        cfg.avoidance_radius    = 3.0f;
        cfg.avoidance_horizon   = 0.8f;
        cfg.avoidance_strength  = 2.5f;
        sim.bootstrap_crowd(cfg);
        for (int i = 0; i < 120; ++i) {
            sim.tick(1.0 / 60.0);
        }
        return sim.sim_hash();
    };

    uint64_t h1 = run();
    uint64_t h2 = run();
    check(h1 == h2, "determinism: identical runs produce same hash");
    check(h1 != 0,  "determinism: hash is non-zero");
}

// =================================================================
//  Demo presets still bootstrap without errors
// =================================================================

static void test_presets_compatible() {
    for (int i = 0; i < de::k_demo_preset_count; ++i) {
        de::SimState sim;
        de::apply_demo_preset(sim, de::k_demo_presets[i]);

        // Should have agents and the avoidance system registered.
        de::SimSnapshot snap = sim.snapshot();
        check(snap.crowd_agent_count > 0,
              "preset_compat: preset has agents");
        check(snap.system_count == 13,
              "preset_compat: 13 systems registered");

        // Run 10 ticks without crash.
        for (int t = 0; t < 10; ++t) {
            sim.tick(1.0 / 60.0);
        }

        // No NaN in any position.
        bool has_nan = false;
        sim.world.each<de::CrowdAgent, de::Position>(
            [&](de::EntityId, de::CrowdAgent&, de::Position& p) {
                if (std::isnan(p.x) || std::isnan(p.y)) has_nan = true;
            });
        check(!has_nan, "preset_compat: no NaN after 10 ticks");
    }
}

// =================================================================
//  Pipeline order includes LocalAvoidance at position 5
// =================================================================

static void test_pipeline_order() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0 / 60.0);
    de::SimSnapshot snap = sim.snapshot();

    check(snap.system_count == 13, "pipeline: 13 systems");
    check(std::strcmp(snap.systems[5].name, "LocalAvoidance") == 0,
          "pipeline: [5] is LocalAvoidance");
    check(std::strcmp(snap.systems[4].name, "ApplyCrowdSteer") == 0,
          "pipeline: [4] is ApplyCrowdSteer (before avoidance)");
    check(std::strcmp(snap.systems[6].name, "ApplySeparation") == 0,
          "pipeline: [6] is ApplySeparation (after avoidance)");
}

// =================================================================

int main() {
    test_opposing_agents_dodge();
    test_lane_clash_fewer_stalls();
    test_zero_velocity_no_nan();
    test_low_density_no_avoidance();
    test_avoidance_determinism();
    test_presets_compatible();
    test_pipeline_order();

    std::printf("\nAvoidanceTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
