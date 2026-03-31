#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"
#include "Runtime/CrowdSystems.h"
#include "Runtime/SpatialGrid.h"

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
//  Sparse scene: agents far apart, few melee candidates
// =================================================================

static void test_sparse_melee() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team    = 5;
    cfg.team_spacing       = 100.0f;  // very far apart
    cfg.agent_spread       = 2.0f;
    cfg.attack_range       = 2.0f;
    cfg.engage_radius      = 15.0f;
    sim.bootstrap_crowd(cfg);

    // One tick -- teams are 200 units apart, no melee should happen.
    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    check(snap.melee_pairs_this_tick == 0,
          "sparse: no melee pairs when teams far apart");
    check(snap.melee_attacks_this_tick == 0,
          "sparse: no melee attacks when teams far apart");
    check(snap.melee_broadphase_checks == 0,
          "sparse: broadphase checks zero when nobody in range");
}

// =================================================================
//  Dense scene: agents close, many melee candidates
// =================================================================

static void test_dense_melee() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team    = 10;
    cfg.team_spacing       = 1.0f;   // teams overlap
    cfg.agent_spread       = 0.5f;
    cfg.attack_range       = 5.0f;   // generous range
    cfg.engage_radius      = 20.0f;
    cfg.attack_interval    = 0.5f;
    sim.bootstrap_crowd(cfg);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    // With overlapping teams and generous range, broadphase must find pairs.
    check(snap.melee_pairs_this_tick > 0,
          "dense: melee pairs generated when teams overlap");
    check(snap.melee_broadphase_checks > 0,
          "dense: broadphase checks performed");
    // Pairs must be bounded: at most N_agents * max_neighbors_per_agent.
    // With 20 agents, pairs cannot exceed 20*19 = 380.
    check(snap.melee_pairs_this_tick <= 20 * 19,
          "dense: melee pairs bounded by agent count");
}

// =================================================================
//  Only enemies appear in melee pairs (no friendly fire)
// =================================================================

static void test_melee_enemies_only() {
    de::World world;
    de::CommandBuffer cmds;

    // Create 2 allies (team 0) very close together.
    de::EntityId a1 = world.create();
    world.set(a1, de::CrowdAgent{});
    world.set(a1, de::Team{0});
    world.set(a1, de::Position{0.0f, 0.0f});
    world.set(a1, de::Velocity{});
    world.set(a1, de::MoveSpeed{3.0f});
    world.set(a1, de::Target{});
    world.set(a1, de::DesiredDirection{});
    world.set(a1, de::Health{100.0f, 100.0f});
    world.set(a1, de::AttackRange{5.0f});
    world.set(a1, de::AttackDamage{10.0f});
    world.set(a1, de::AttackCooldown{0.0f, 1.0f});
    world.set(a1, de::BattleGoal{10.0f, 0.0f});
    world.set(a1, de::EngageRadius{15.0f});
    world.set(a1, de::Separation{0.8f, 5.0f});
    world.set(a1, de::BehaviorLod{});

    de::EntityId a2 = world.create();
    world.set(a2, de::CrowdAgent{});
    world.set(a2, de::Team{0});
    world.set(a2, de::Position{1.0f, 0.0f});
    world.set(a2, de::Velocity{});
    world.set(a2, de::MoveSpeed{3.0f});
    world.set(a2, de::Target{});
    world.set(a2, de::DesiredDirection{});
    world.set(a2, de::Health{100.0f, 100.0f});
    world.set(a2, de::AttackRange{5.0f});
    world.set(a2, de::AttackDamage{10.0f});
    world.set(a2, de::AttackCooldown{0.0f, 1.0f});
    world.set(a2, de::BattleGoal{10.0f, 0.0f});
    world.set(a2, de::EngageRadius{15.0f});
    world.set(a2, de::Separation{0.8f, 5.0f});
    world.set(a2, de::BehaviorLod{});

    // Run select_targets (builds grid) then broadphase.
    de::reset_crowd_tick_counters();
    de::set_crowd_tick_count(0);
    de::WorldView view(world);
    de::select_targets(view, 0.016f, cmds);
    de::gather_melee_candidates(view, 0.016f, cmds);

    // Two allies within range, but no enemy -> 0 melee pairs.
    check(de::melee_pairs_this_tick() == 0,
          "enemies_only: no melee pairs between allies");
}

// =================================================================
//  Melee pair with one enemy in range produces an attack
// =================================================================

static void test_melee_produces_attack() {
    de::World world;
    de::CommandBuffer cmds;

    // Team 0 agent at origin.
    de::EntityId a = world.create();
    world.set(a, de::CrowdAgent{});
    world.set(a, de::Team{0});
    world.set(a, de::Position{0.0f, 0.0f});
    world.set(a, de::Velocity{});
    world.set(a, de::MoveSpeed{3.0f});
    world.set(a, de::Target{});
    world.set(a, de::DesiredDirection{});
    world.set(a, de::Health{100.0f, 100.0f});
    world.set(a, de::AttackRange{3.0f});
    world.set(a, de::AttackDamage{25.0f});
    world.set(a, de::AttackCooldown{0.0f, 1.0f});
    world.set(a, de::BattleGoal{10.0f, 0.0f});
    world.set(a, de::EngageRadius{15.0f});
    world.set(a, de::Separation{0.8f, 5.0f});
    world.set(a, de::BehaviorLod{});

    // Team 1 enemy at (2, 0) -- within attack range of 3.
    de::EntityId e = world.create();
    world.set(e, de::CrowdAgent{});
    world.set(e, de::Team{1});
    world.set(e, de::Position{2.0f, 0.0f});
    world.set(e, de::Velocity{});
    world.set(e, de::MoveSpeed{3.0f});
    world.set(e, de::Target{});
    world.set(e, de::DesiredDirection{});
    world.set(e, de::Health{100.0f, 100.0f});
    world.set(e, de::AttackRange{3.0f});
    world.set(e, de::AttackDamage{25.0f});
    world.set(e, de::AttackCooldown{0.0f, 1.0f});
    world.set(e, de::BattleGoal{-10.0f, 0.0f});
    world.set(e, de::EngageRadius{15.0f});
    world.set(e, de::Separation{0.8f, 5.0f});
    world.set(e, de::BehaviorLod{});

    de::reset_crowd_tick_counters();
    de::set_crowd_tick_count(0);
    de::WorldView view(world);

    // Build grid, broadphase, then attack.
    de::select_targets(view, 0.016f, cmds);
    de::gather_melee_candidates(view, 0.016f, cmds);
    de::attack_targets(view, 0.016f, cmds);
    de::resolve_damage(view, 0.016f, cmds);

    check(de::melee_pairs_this_tick() == 2,
          "produces_attack: 2 melee pairs (symmetric)");
    check(de::melee_attacks_this_tick() >= 1,
          "produces_attack: at least 1 attack resolved");

    // Both took damage (simultaneous).
    auto* hp_a = view.get<de::Health>(a);
    auto* hp_e = view.get<de::Health>(e);
    check(hp_a && hp_a->current < 100.0f,
          "produces_attack: agent a took damage");
    check(hp_e && hp_e->current < 100.0f,
          "produces_attack: enemy e took damage");
}

// =================================================================
//  Simultaneous combat contract: both agents act even if damage kills
// =================================================================

static void test_simultaneous_combat() {
    de::World world;
    de::CommandBuffer cmds;

    // Two agents with 25 HP each, dealing 50 damage.
    // Both should attack (simultaneous) and both die after resolve.
    de::EntityId a = world.create();
    world.set(a, de::CrowdAgent{});
    world.set(a, de::Team{0});
    world.set(a, de::Position{0.0f, 0.0f});
    world.set(a, de::Velocity{});
    world.set(a, de::MoveSpeed{3.0f});
    world.set(a, de::Target{});
    world.set(a, de::DesiredDirection{});
    world.set(a, de::Health{25.0f, 25.0f});
    world.set(a, de::AttackRange{3.0f});
    world.set(a, de::AttackDamage{50.0f});
    world.set(a, de::AttackCooldown{0.0f, 1.0f});
    world.set(a, de::BattleGoal{10.0f, 0.0f});
    world.set(a, de::EngageRadius{15.0f});
    world.set(a, de::Separation{0.8f, 5.0f});
    world.set(a, de::BehaviorLod{});

    de::EntityId b = world.create();
    world.set(b, de::CrowdAgent{});
    world.set(b, de::Team{1});
    world.set(b, de::Position{1.0f, 0.0f});
    world.set(b, de::Velocity{});
    world.set(b, de::MoveSpeed{3.0f});
    world.set(b, de::Target{});
    world.set(b, de::DesiredDirection{});
    world.set(b, de::Health{25.0f, 25.0f});
    world.set(b, de::AttackRange{3.0f});
    world.set(b, de::AttackDamage{50.0f});
    world.set(b, de::AttackCooldown{0.0f, 1.0f});
    world.set(b, de::BattleGoal{-10.0f, 0.0f});
    world.set(b, de::EngageRadius{15.0f});
    world.set(b, de::Separation{0.8f, 5.0f});
    world.set(b, de::BehaviorLod{});

    de::reset_crowd_tick_counters();
    de::set_crowd_tick_count(0);
    de::WorldView view(world);

    de::select_targets(view, 0.016f, cmds);
    de::gather_melee_candidates(view, 0.016f, cmds);
    de::attack_targets(view, 0.016f, cmds);

    // Both must attack before damage resolves.
    check(de::melee_attacks_this_tick() == 2,
          "simultaneous: both agents attacked");

    de::resolve_damage(view, 0.016f, cmds);

    // Both took lethal damage.
    auto* hp_a = view.get<de::Health>(a);
    auto* hp_b = view.get<de::Health>(b);
    check(hp_a && hp_a->current <= 0.0f,
          "simultaneous: agent a is dead");
    check(hp_b && hp_b->current <= 0.0f,
          "simultaneous: agent b is dead");
}

// =================================================================
//  Integration: full pipeline via SimState preserves behavior
// =================================================================

static void test_pipeline_integration() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 3.0f;   // close enough to engage
    cfg.agent_spread    = 1.0f;
    cfg.attack_range    = 5.0f;
    cfg.engage_radius   = 20.0f;
    cfg.attack_interval = 0.5f;
    sim.bootstrap_crowd(cfg);

    // Run several ticks.
    for (int i = 0; i < 60; ++i) {
        sim.tick(1.0 / 60.0);
    }
    auto snap = sim.snapshot();

    // Pipeline must contain MeleeBroadphase (names filled after first tick).
    bool found_bp = false;
    for (uint32_t i = 0; i < snap.system_count; ++i) {
        if (std::strcmp(snap.systems[i].name, "MeleeBroadphase") == 0) {
            found_bp = true;
        }
    }
    check(found_bp, "pipeline: MeleeBroadphase system registered");

    // After 60 ticks at close range, there should have been attacks.
    // (Snapshot only shows last-tick values, but melee_pairs should be > 0
    //  on the last tick if agents are still alive and close.)
    check(snap.melee_broadphase_checks > 0 || snap.crowd_agent_count == 0,
          "pipeline: broadphase ran during simulation");

    // Verify system order: MeleeBroadphase must come before AttackTargets.
    int bp_idx  = -1;
    int atk_idx = -1;
    for (uint32_t i = 0; i < snap.system_count; ++i) {
        if (std::strcmp(snap.systems[i].name, "MeleeBroadphase") == 0)
            bp_idx = static_cast<int>(i);
        if (std::strcmp(snap.systems[i].name, "AttackTargets") == 0)
            atk_idx = static_cast<int>(i);
    }
    check(bp_idx >= 0 && atk_idx >= 0 && bp_idx < atk_idx,
          "pipeline: MeleeBroadphase runs before AttackTargets");
}

// =================================================================
//  Telemetry: snapshot melee fields are coherent
// =================================================================

static void test_melee_telemetry_coherent() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 1.5f;
    cfg.agent_spread    = 0.5f;
    cfg.attack_range    = 4.0f;
    cfg.engage_radius   = 20.0f;
    sim.bootstrap_crowd(cfg);

    sim.tick(1.0 / 60.0);
    auto snap = sim.snapshot();

    // melee_attacks <= melee_pairs (can't attack more pairs than exist).
    check(snap.melee_attacks_this_tick <= snap.melee_pairs_this_tick,
          "telemetry: attacks <= pairs");

    // melee_attacks == attacks_this_tick (same source now).
    check(snap.melee_attacks_this_tick == snap.attacks_this_tick,
          "telemetry: melee_attacks matches attacks_this_tick");
}

// =================================================================
//  Existing crowd tests must not regress: bootstrap + tick cycle
// =================================================================

static void test_existing_crowd_no_regression() {
    de::SimState sim;
    sim.bootstrap_crowd();

    // 20 agents, default config.
    check(sim.world.entity_count() == 20,
          "regression: 20 agents after bootstrap");

    for (int i = 0; i < 10; ++i) {
        sim.tick(1.0 / 60.0);
    }

    // Agents should still exist (10 ticks at default spacing is not enough
    // for any deaths with default damage/hp/interval).
    auto snap = sim.snapshot();
    check(snap.crowd_agent_count == 20,
          "regression: all 20 agents alive after 10 ticks");
    check(snap.system_count == 12,
          "regression: 12 systems registered (with MeleeBroadphase)");
}

// =================================================================

int main() {
    test_sparse_melee();
    test_dense_melee();
    test_melee_enemies_only();
    test_melee_produces_attack();
    test_simultaneous_combat();
    test_pipeline_integration();
    test_melee_telemetry_coherent();
    test_existing_crowd_no_regression();

    std::printf("\nMeleeTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
