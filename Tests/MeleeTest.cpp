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
//  Attacker with 2 enemies in range attacks Target.entity, not a
//  random broadphase neighbor.
// =================================================================

static void test_attack_respects_target() {
    de::World world;
    de::CommandBuffer cmds;

    // Attacker (team 0) at origin, targeting enemy_far.
    de::EntityId atk = world.create();
    world.set(atk, de::CrowdAgent{});
    world.set(atk, de::Team{0});
    world.set(atk, de::Position{0.0f, 0.0f});
    world.set(atk, de::Velocity{});
    world.set(atk, de::MoveSpeed{3.0f});
    world.set(atk, de::DesiredDirection{});
    world.set(atk, de::Health{100.0f, 100.0f});
    world.set(atk, de::AttackRange{5.0f});
    world.set(atk, de::AttackDamage{20.0f});
    world.set(atk, de::AttackCooldown{0.0f, 1.0f});
    world.set(atk, de::BattleGoal{10.0f, 0.0f});
    world.set(atk, de::EngageRadius{15.0f});
    world.set(atk, de::Separation{0.8f, 5.0f});
    world.set(atk, de::BehaviorLod{});

    // Enemy close (team 1) at (1, 0) -- in range.
    de::EntityId enemy_close = world.create();
    world.set(enemy_close, de::CrowdAgent{});
    world.set(enemy_close, de::Team{1});
    world.set(enemy_close, de::Position{1.0f, 0.0f});
    world.set(enemy_close, de::Velocity{});
    world.set(enemy_close, de::MoveSpeed{3.0f});
    world.set(enemy_close, de::Target{});
    world.set(enemy_close, de::DesiredDirection{});
    world.set(enemy_close, de::Health{100.0f, 100.0f});
    world.set(enemy_close, de::AttackRange{5.0f});
    world.set(enemy_close, de::AttackDamage{10.0f});
    world.set(enemy_close, de::AttackCooldown{0.0f, 1.0f});
    world.set(enemy_close, de::BattleGoal{-10.0f, 0.0f});
    world.set(enemy_close, de::EngageRadius{15.0f});
    world.set(enemy_close, de::Separation{0.8f, 5.0f});
    world.set(enemy_close, de::BehaviorLod{});

    // Enemy far (team 1) at (4, 0) -- also in range (< 5).
    de::EntityId enemy_far = world.create();
    world.set(enemy_far, de::CrowdAgent{});
    world.set(enemy_far, de::Team{1});
    world.set(enemy_far, de::Position{4.0f, 0.0f});
    world.set(enemy_far, de::Velocity{});
    world.set(enemy_far, de::MoveSpeed{3.0f});
    world.set(enemy_far, de::Target{});
    world.set(enemy_far, de::DesiredDirection{});
    world.set(enemy_far, de::Health{100.0f, 100.0f});
    world.set(enemy_far, de::AttackRange{5.0f});
    world.set(enemy_far, de::AttackDamage{10.0f});
    world.set(enemy_far, de::AttackCooldown{0.0f, 1.0f});
    world.set(enemy_far, de::BattleGoal{-10.0f, 0.0f});
    world.set(enemy_far, de::EngageRadius{15.0f});
    world.set(enemy_far, de::Separation{0.8f, 5.0f});
    world.set(enemy_far, de::BehaviorLod{});

    // Manually set attacker's target to enemy_far (NOT the closest).
    world.set(atk, de::Target{enemy_far, true});

    de::reset_crowd_tick_counters();
    de::set_crowd_tick_count(0);
    de::WorldView view(world);

    // Build grid + broadphase (both enemies in range).
    de::select_targets(view, 0.016f, cmds);

    // select_targets would normally pick nearest enemy, overriding our
    // manual target.  Force target back to enemy_far to test the contract.
    auto* tgt = view.get<de::Target>(atk);
    tgt->entity     = enemy_far;
    tgt->has_target = true;

    de::gather_melee_candidates(view, 0.016f, cmds);

    // Both enemies should be broadphase candidates.
    check(de::melee_pairs_this_tick() >= 2,
          "respects_target: broadphase found both enemies");

    de::attack_targets(view, 0.016f, cmds);
    de::resolve_damage(view, 0.016f, cmds);

    // Only enemy_far (the actual Target) should have taken damage.
    auto* hp_close = view.get<de::Health>(enemy_close);
    auto* hp_far   = view.get<de::Health>(enemy_far);
    check(hp_close && hp_close->current == 100.0f,
          "respects_target: enemy_close NOT hit (not the target)");
    check(hp_far && hp_far->current == 80.0f,
          "respects_target: enemy_far hit for 20 damage (is the target)");
}

// =================================================================
//  interval == 0 does not cause multi-hit in the same tick
// =================================================================

static void test_zero_interval_single_hit() {
    de::World world;
    de::CommandBuffer cmds;

    // Attacker with interval = 0.
    de::EntityId atk = world.create();
    world.set(atk, de::CrowdAgent{});
    world.set(atk, de::Team{0});
    world.set(atk, de::Position{0.0f, 0.0f});
    world.set(atk, de::Velocity{});
    world.set(atk, de::MoveSpeed{3.0f});
    world.set(atk, de::Target{});
    world.set(atk, de::DesiredDirection{});
    world.set(atk, de::Health{100.0f, 100.0f});
    world.set(atk, de::AttackRange{5.0f});
    world.set(atk, de::AttackDamage{30.0f});
    world.set(atk, de::AttackCooldown{0.0f, 0.0f});  // interval = 0!
    world.set(atk, de::BattleGoal{10.0f, 0.0f});
    world.set(atk, de::EngageRadius{15.0f});
    world.set(atk, de::Separation{0.8f, 5.0f});
    world.set(atk, de::BehaviorLod{});

    // Enemy in range.
    de::EntityId enemy = world.create();
    world.set(enemy, de::CrowdAgent{});
    world.set(enemy, de::Team{1});
    world.set(enemy, de::Position{1.0f, 0.0f});
    world.set(enemy, de::Velocity{});
    world.set(enemy, de::MoveSpeed{3.0f});
    world.set(enemy, de::Target{});
    world.set(enemy, de::DesiredDirection{});
    world.set(enemy, de::Health{100.0f, 100.0f});
    world.set(enemy, de::AttackRange{5.0f});
    world.set(enemy, de::AttackDamage{10.0f});
    world.set(enemy, de::AttackCooldown{0.0f, 1.0f});
    world.set(enemy, de::BattleGoal{-10.0f, 0.0f});
    world.set(enemy, de::EngageRadius{15.0f});
    world.set(enemy, de::Separation{0.8f, 5.0f});
    world.set(enemy, de::BehaviorLod{});

    de::reset_crowd_tick_counters();
    de::set_crowd_tick_count(0);
    de::WorldView view(world);

    de::select_targets(view, 0.016f, cmds);
    de::gather_melee_candidates(view, 0.016f, cmds);
    de::attack_targets(view, 0.016f, cmds);
    de::resolve_damage(view, 0.016f, cmds);

    // Attacker should have emitted exactly 1 hit despite interval == 0.
    // (enemy also attacks back -> 2 total, but attacker contributes 1).
    auto* hp_enemy = view.get<de::Health>(enemy);
    check(hp_enemy && hp_enemy->current == 70.0f,
          "zero_interval: enemy took exactly 30 damage (1 hit, not multi)");

    // Run a second tick -- attacker should hit again (interval=0, ready
    // immediately) but still only once.
    de::reset_crowd_tick_counters();
    de::set_crowd_tick_count(1);
    de::select_targets(view, 0.016f, cmds);
    de::gather_melee_candidates(view, 0.016f, cmds);
    de::attack_targets(view, 0.016f, cmds);
    de::resolve_damage(view, 0.016f, cmds);

    auto* hp2 = view.get<de::Health>(enemy);
    check(hp2 && hp2->current == 40.0f,
          "zero_interval: enemy took exactly 30 more on second tick");
}

// =================================================================
//  Simultaneous contract preserved with target-gated broadphase:
//  both agents with lethal damage still both attack.
// =================================================================

static void test_simultaneous_with_target_gate() {
    de::World world;
    de::CommandBuffer cmds;

    // A (team 0) and B (team 1) at distance 1, both deal lethal damage.
    de::EntityId a = world.create();
    world.set(a, de::CrowdAgent{});
    world.set(a, de::Team{0});
    world.set(a, de::Position{0.0f, 0.0f});
    world.set(a, de::Velocity{});
    world.set(a, de::MoveSpeed{3.0f});
    world.set(a, de::Target{});
    world.set(a, de::DesiredDirection{});
    world.set(a, de::Health{10.0f, 10.0f});
    world.set(a, de::AttackRange{3.0f});
    world.set(a, de::AttackDamage{999.0f});
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
    world.set(b, de::Health{10.0f, 10.0f});
    world.set(b, de::AttackRange{3.0f});
    world.set(b, de::AttackDamage{999.0f});
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

    // Both must have attacked (2 hits total).
    check(de::melee_attacks_this_tick() == 2,
          "simultaneous_gate: both agents attacked before resolve");

    de::resolve_damage(view, 0.016f, cmds);

    auto* hp_a = view.get<de::Health>(a);
    auto* hp_b = view.get<de::Health>(b);
    check(hp_a && hp_a->current <= 0.0f,
          "simultaneous_gate: a is dead after simultaneous resolve");
    check(hp_b && hp_b->current <= 0.0f,
          "simultaneous_gate: b is dead after simultaneous resolve");
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
    test_attack_respects_target();
    test_zero_interval_single_hit();
    test_simultaneous_with_target_gate();

    std::printf("\nMeleeTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
