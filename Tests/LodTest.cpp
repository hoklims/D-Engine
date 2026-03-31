#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"

#include <cmath>
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

// =================================================================
//  All agents get a BehaviorLod component at bootstrap
// =================================================================

static void test_lod_component_present() {
    de::SimState sim;
    sim.bootstrap_crowd();

    uint32_t with_lod = 0;
    sim.world.each<de::CrowdAgent, de::BehaviorLod>(
        [&](de::EntityId, de::CrowdAgent&, de::BehaviorLod&) {
            ++with_lod;
        });

    check(with_lod == 20, "lod_present: all 20 agents have BehaviorLod");
}

// =================================================================
//  After 1 tick, all close agents are classified T0
// =================================================================

static void test_lod_close_agents_t0() {
    // Default scene: teams at x=-20 and x=+20.
    // Default t1_distance=30, so all agents within 30 of center -> T0.
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    bool all_t0 = true;
    sim.world.each<de::CrowdAgent, de::BehaviorLod>(
        [&](de::EntityId, de::CrowdAgent&, de::BehaviorLod& lod) {
            if (lod.tier != 0) all_t0 = false;
        });

    check(all_t0, "lod_close_t0: all agents at dist < 30 are T0");

    de::SimSnapshot snap = sim.snapshot();
    check(snap.lod_tier_counts[0] == 20,
          "lod_close_t0: snapshot shows 20 agents in T0");
    check(snap.lod_tier_counts[1] == 0,
          "lod_close_t0: snapshot shows 0 agents in T1");
}

// =================================================================
//  Distant agents get downgraded to higher tiers
// =================================================================

static void test_lod_distant_agents_downgraded() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 80.0f;  // agents at x=-80 and x=+80
    cfg.engage_radius   = 2.0f;   // tiny, no engagement at that distance

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0);

    // Agents at x=+-80, y=0..8.  Distance to center ~ 80.
    // With default t1=30, t2=60, t3=100 -> all should be T2 (60..100).
    uint32_t t2_count = 0;
    sim.world.each<de::CrowdAgent, de::BehaviorLod>(
        [&](de::EntityId, de::CrowdAgent&, de::BehaviorLod& lod) {
            if (lod.tier == 2) ++t2_count;
        });

    check(t2_count == 10,
          "lod_distant: all 10 agents at dist ~80 are T2");

    de::SimSnapshot snap = sim.snapshot();
    check(snap.lod_tier_counts[2] == 10,
          "lod_distant: snapshot shows 10 agents in T2");
}

// =================================================================
//  Very distant agents are T3
// =================================================================

static void test_lod_very_distant_t3() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 3;
    cfg.team_spacing    = 120.0f;  // agents at x=+-120
    cfg.engage_radius   = 1.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0);

    uint32_t t3_count = 0;
    sim.world.each<de::CrowdAgent, de::BehaviorLod>(
        [&](de::EntityId, de::CrowdAgent&, de::BehaviorLod& lod) {
            if (lod.tier == 3) ++t3_count;
        });

    check(t3_count == 6,
          "lod_very_distant: all 6 agents at dist ~120 are T3");
}

// =================================================================
//  Engaged agent stays T0 even if far away
// =================================================================

static void test_lod_engaged_stays_t0() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 80.0f;   // far from center -> would be T2
    cfg.engage_radius   = 200.0f;  // huge -> always engaged
    cfg.attack_range    = 1.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0);

    // Both agents are far from center but engaged -> T0.
    bool all_t0 = true;
    sim.world.each<de::CrowdAgent, de::BehaviorLod>(
        [&](de::EntityId, de::CrowdAgent&, de::BehaviorLod& lod) {
            if (lod.tier != 0) all_t0 = false;
        });

    check(all_t0,
          "lod_engaged_t0: engaged agents stay T0 despite distance");
}

// =================================================================
//  LOD gating reduces entities_processed in gated systems
// =================================================================

static void test_lod_gating_reduces_work() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 80.0f;  // all agents T2 (stride=4)
    cfg.engage_radius   = 1.0f;   // no engagement

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Tick 0: stride=4, tick%4==0 -> all agents processed.
    sim.tick(1.0);
    de::SimSnapshot snap0 = sim.snapshot();

    // ComputeBattleGoal is system [2] (after ClassifyLod and SelectTargets).
    uint32_t processed_tick0 = snap0.systems[2].entities_processed;
    check(processed_tick0 == 10,
          "lod_gating: tick 0 processes all 10 agents in ComputeBattleGoal");

    // Tick 1: tick%4 != 0 -> T2 agents skipped.
    sim.tick(1.0);
    de::SimSnapshot snap1 = sim.snapshot();
    uint32_t processed_tick1 = snap1.systems[2].entities_processed;
    check(processed_tick1 == 0,
          "lod_gating: tick 1 processes 0 agents (all T2, stride=4)");

    // Skipped counter should be > 0.
    check(snap1.lod_skipped_this_tick > 0,
          "lod_gating: lod_skipped > 0 on off-tick");
}

// =================================================================
//  T0 agents are never skipped regardless of tick
// =================================================================

static void test_lod_t0_never_skipped() {
    de::SimState sim;
    sim.bootstrap_crowd();  // default: all close -> T0

    // Run 4 ticks, every tick should process all agents in ComputeBattleGoal.
    for (int i = 0; i < 4; ++i) {
        sim.tick(1.0);
        de::SimSnapshot snap = sim.snapshot();
        // ComputeBattleGoal at index [2].
        check(snap.systems[2].entities_processed == 20,
              "lod_t0_always: all 20 T0 agents processed every tick");
    }
}

// =================================================================
//  Combat systems run for ALL agents regardless of LOD tier
// =================================================================

static void test_lod_combat_not_gated() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 80.0f;   // T2 agents
    cfg.engage_radius   = 1.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Place two enemies within attack range to force combat.
    de::EntityId a = {0, 1};
    de::EntityId b = {5, 1};
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;

    // Tick 1: combat should work even though these agents are T2.
    sim.tick(1.0);

    // AttackTargets is at index [6].  It should process all agents.
    de::SimSnapshot snap = sim.snapshot();
    check(snap.systems[6].entities_processed == 10,
          "lod_combat: AttackTargets processes all 10 agents (not gated)");
}

// =================================================================
//  Snapshot LOD telemetry is coherent
// =================================================================

static void test_lod_snapshot_telemetry() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    uint32_t total = 0;
    for (int t = 0; t < 4; ++t) total += snap.lod_tier_counts[t];
    check(total == 20,
          "lod_telemetry: tier counts sum to 20");
    check(snap.lod_skipped_this_tick == 0,
          "lod_telemetry: no skips when all T0");
}

// =================================================================
//  Main
// =================================================================

int main() {
    test_lod_component_present();
    test_lod_close_agents_t0();
    test_lod_distant_agents_downgraded();
    test_lod_very_distant_t3();
    test_lod_engaged_stays_t0();
    test_lod_gating_reduces_work();
    test_lod_t0_never_skipped();
    test_lod_combat_not_gated();
    test_lod_snapshot_telemetry();

    std::printf("\n--- LodTest: %d passed, %d failed ---\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
