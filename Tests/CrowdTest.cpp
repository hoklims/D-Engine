#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"
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

static bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabsf(a - b) < eps;
}

// =================================================================
//  bootstrap_crowd creates the expected agent count
// =================================================================

static void test_crowd_bootstrap_count() {
    de::SimState sim;
    sim.bootstrap_crowd();

    check(sim.world.entity_count() == 20,
          "crowd_bootstrap: 20 entities (2 teams x 10)");
}

// =================================================================
//  Teams are correctly distributed
// =================================================================

static void test_crowd_team_distribution() {
    de::SimState sim;
    sim.bootstrap_crowd();

    uint32_t team0 = 0;
    uint32_t team1 = 0;
    sim.world.each<de::CrowdAgent, de::Team>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t) {
            if (t.id == 0) ++team0;
            if (t.id == 1) ++team1;
        });

    check(team0 == 10, "team_distribution: team 0 has 10 agents");
    check(team1 == 10, "team_distribution: team 1 has 10 agents");
}

// =================================================================
//  All agents have all required components
// =================================================================

static void test_crowd_components_present() {
    de::SimState sim;
    sim.bootstrap_crowd();

    uint32_t complete = 0;
    sim.world.each<de::CrowdAgent, de::Team, de::Position, de::Velocity,
                   de::MoveSpeed, de::Target, de::DesiredDirection, de::Health>(
        [&](de::EntityId, de::CrowdAgent&, de::Team&, de::Position&,
            de::Velocity&, de::MoveSpeed&, de::Target&,
            de::DesiredDirection&, de::Health&) {
            ++complete;
        });

    check(complete == 20, "components_present: all 20 agents have full component set");
}

// =================================================================
//  Initial positions: team 0 at x=-20, team 1 at x=+20
// =================================================================

static void test_crowd_initial_positions() {
    de::SimState sim;
    sim.bootstrap_crowd();

    bool team0_ok = true;
    bool team1_ok = true;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0 && !approx(p.x, -20.0f)) team0_ok = false;
            if (t.id == 1 && !approx(p.x,  20.0f)) team1_ok = false;
        });

    check(team0_ok, "initial_positions: team 0 at x=-20");
    check(team1_ok, "initial_positions: team 1 at x=+20");
}

// =================================================================
//  After 1 tick, all agents have a valid target
// =================================================================

static void test_crowd_target_selection() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    uint32_t with_target = 0;
    sim.world.each<de::CrowdAgent, de::Target>(
        [&](de::EntityId, de::CrowdAgent&, de::Target& tgt) {
            if (tgt.has_target) ++with_target;
        });

    check(with_target == 20, "target_selection: all 20 agents have a target after 1 tick");
}

// =================================================================
//  Targets are on the enemy team
// =================================================================

static void test_crowd_target_is_enemy() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    bool all_enemy = true;
    sim.world.each<de::CrowdAgent, de::Team, de::Target>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& my_team, de::Target& tgt) {
            if (!tgt.has_target) { all_enemy = false; return; }
            auto* enemy_team = sim.world.get<de::Team>(tgt.entity);
            if (!enemy_team || enemy_team->id == my_team.id) {
                all_enemy = false;
            }
        });

    check(all_enemy, "target_is_enemy: every target is on the opposing team");
}

// =================================================================
//  Desired direction is coherent (team 0 -> +x, team 1 -> -x)
// =================================================================

static void test_crowd_desired_direction() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    bool dir_ok = true;
    sim.world.each<de::CrowdAgent, de::Team, de::DesiredDirection>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::DesiredDirection& d) {
            // Team 0 is at x=-20 targeting x=+20 -> dx should be positive.
            // Team 1 is at x=+20 targeting x=-20 -> dx should be negative.
            if (t.id == 0 && d.dx <= 0.0f) dir_ok = false;
            if (t.id == 1 && d.dx >= 0.0f) dir_ok = false;
        });

    check(dir_ok, "desired_direction: team 0 -> +x, team 1 -> -x");
}

// =================================================================
//  Multiple ticks converge agents toward each other
// =================================================================

static void test_crowd_convergence() {
    de::SimState sim;
    sim.bootstrap_crowd();

    // Measure initial gap: team 0 at x=-20, team 1 at x=+20 -> gap = 40.
    float initial_gap = 40.0f;

    constexpr int ticks = 5;
    for (int i = 0; i < ticks; ++i) {
        sim.tick(1.0);
    }

    // After 5 ticks at speed 3.0 per side, gap should decrease by ~30.
    float min_x_team1 = 1e6f;
    float max_x_team0 = -1e6f;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0 && p.x > max_x_team0) max_x_team0 = p.x;
            if (t.id == 1 && p.x < min_x_team1) min_x_team1 = p.x;
        });

    float current_gap = min_x_team1 - max_x_team0;

    check(current_gap < initial_gap,
          "convergence: gap decreased after 5 ticks");
    check(current_gap < 15.0f,
          "convergence: gap < 15 after 5 ticks at speed 3.0");
}

// =================================================================
//  System pipeline has correct order (5 systems)
// =================================================================

static void test_crowd_system_order() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.system_count == 11, "system_order: 11 systems registered");

    check(std::strcmp(snap.systems[0].name, "SelectTargets") == 0,
          "system_order: [0] SelectTargets");
    check(std::strcmp(snap.systems[1].name, "ClassifyLod") == 0,
          "system_order: [1] ClassifyLod");
    check(std::strcmp(snap.systems[2].name, "ComputeBattleGoal") == 0,
          "system_order: [2] ComputeBattleGoal");
    check(std::strcmp(snap.systems[3].name, "ComputeDesiredMove") == 0,
          "system_order: [3] ComputeDesiredMove");
    check(std::strcmp(snap.systems[4].name, "ApplyCrowdSteer") == 0,
          "system_order: [4] ApplyCrowdSteer");
    check(std::strcmp(snap.systems[5].name, "ApplySeparation") == 0,
          "system_order: [5] ApplySeparation");
    check(std::strcmp(snap.systems[6].name, "AttackTargets") == 0,
          "system_order: [6] AttackTargets");
    check(std::strcmp(snap.systems[7].name, "ResolveDamage") == 0,
          "system_order: [7] ResolveDamage");
    check(std::strcmp(snap.systems[8].name, "RemoveDead") == 0,
          "system_order: [8] RemoveDead");
    check(std::strcmp(snap.systems[9].name, "IntegrateVelocity") == 0,
          "system_order: [9] IntegrateVelocity");
    check(std::strcmp(snap.systems[10].name, "IntegratePosition") == 0,
          "system_order: [10] IntegratePosition");
}

// =================================================================
//  Snapshot crowd metrics are coherent
// =================================================================

static void test_crowd_snapshot_metrics() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::SimSnapshot snap0 = sim.snapshot();
    check(snap0.crowd_agent_count == 20,
          "snapshot_metrics: 20 crowd agents before tick");
    check(snap0.agents_with_target == 0,
          "snapshot_metrics: 0 targets before tick");
    check(snap0.team_counts[0] == 10,
          "snapshot_metrics: team 0 count == 10");
    check(snap0.team_counts[1] == 10,
          "snapshot_metrics: team 1 count == 10");

    sim.tick(1.0);

    de::SimSnapshot snap1 = sim.snapshot();
    check(snap1.crowd_agent_count == 20,
          "snapshot_metrics: 20 crowd agents after tick");
    check(snap1.agents_with_target == 20,
          "snapshot_metrics: 20 targets after tick");
    check(snap1.entity_count == 20,
          "snapshot_metrics: entity_count == 20");
    check(snap1.tick_count == 1,
          "snapshot_metrics: tick_count == 1");
}

// =================================================================
//  Velocity is clamped to MoveSpeed
// =================================================================

static void test_crowd_speed_clamp() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    bool speed_ok = true;
    sim.world.each<de::CrowdAgent, de::Velocity, de::MoveSpeed>(
        [&](de::EntityId, de::CrowdAgent&, de::Velocity& v, de::MoveSpeed& spd) {
            float speed = std::sqrt(v.dx * v.dx + v.dy * v.dy);
            if (speed > spd.max + 1e-4f) speed_ok = false;
        });

    check(speed_ok, "speed_clamp: no agent exceeds max speed");
}

// =================================================================
//  bootstrap_crowd is idempotent
// =================================================================

static void test_crowd_bootstrap_idempotent() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);
    sim.tick(1.0);

    sim.bootstrap_crowd();

    check(sim.world.entity_count() == 20,
          "crowd_idempotent: 20 entities after re-bootstrap");

    de::SimSnapshot snap = sim.snapshot();
    check(snap.tick_count == 0,
          "crowd_idempotent: tick_count reset");
    check(snap.system_count == 11,
          "crowd_idempotent: 11 systems, not 22");
    check(snap.crowd_agent_count == 20,
          "crowd_idempotent: crowd metrics correct after re-bootstrap");
}

// =================================================================
//  Agent stops when target is within attack range
// =================================================================

static void test_combat_stop_in_range() {
    de::SimState sim;
    sim.bootstrap_crowd();

    // Move one pair close together (within attack range of 2.0).
    de::EntityId a = {0, 1};   // team 0
    de::EntityId b = {10, 1};  // team 1
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;

    sim.tick(1.0);

    // Both should have near-zero velocity (stopped to fight).
    auto* va = sim.world.get<de::Velocity>(a);
    auto* vb = sim.world.get<de::Velocity>(b);
    check(approx(va->dx, 0.0f) && approx(va->dy, 0.0f),
          "stop_in_range: agent A velocity ~= 0");
    check(approx(vb->dx, 0.0f) && approx(vb->dy, 0.0f),
          "stop_in_range: agent B velocity ~= 0");
}

// =================================================================
//  Attack reduces target HP
// =================================================================

static void test_combat_attack_reduces_hp() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};
    de::EntityId b = {10, 1};
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;

    sim.tick(1.0);

    // Both agents attack each other: damage = 10, so HP = 100 - 10 = 90.
    auto* hp_a = sim.world.get<de::Health>(a);
    auto* hp_b = sim.world.get<de::Health>(b);
    check(approx(hp_a->current, 90.0f),
          "attack_hp: agent A HP == 90 after being attacked");
    check(approx(hp_b->current, 90.0f),
          "attack_hp: agent B HP == 90 after being attacked");
}

// =================================================================
//  Cooldown prevents attacking every tick
// =================================================================

static void test_combat_cooldown() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};
    de::EntityId b = {10, 1};
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    // Agent A: slow attacker (interval 3s). Agent B: never attacks.
    sim.world.get<de::AttackCooldown>(a)->interval = 3.0f;
    sim.world.get<de::AttackCooldown>(b)->remaining = 100.0f;
    sim.world.get<de::AttackCooldown>(b)->interval = 100.0f;

    sim.tick(1.0);  // A attacks B (cd was 0, ready). B cd = 99.
    check(approx(sim.world.get<de::Health>(b)->current, 90.0f),
          "cooldown: tick 1 -- A attacks, B HP == 90");

    sim.tick(1.0);  // A cooldown = 3-1=2 > 0, no attack.
    check(approx(sim.world.get<de::Health>(b)->current, 90.0f),
          "cooldown: tick 2 -- cooldown prevents attack, B HP still 90");

    sim.tick(1.0);  // A cooldown = 2-1=1 > 0, still no attack.
    check(approx(sim.world.get<de::Health>(b)->current, 90.0f),
          "cooldown: tick 3 -- cooldown still active, B HP still 90");

    sim.tick(1.0);  // A cooldown = 1-1=0 <= 0, attack!
    check(approx(sim.world.get<de::Health>(b)->current, 80.0f),
          "cooldown: tick 4 -- cooldown expired, A attacks, B HP == 80");
}

// =================================================================
//  Dead agent is destroyed via CommandBuffer at end of tick
// =================================================================

static void test_combat_deferred_death() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};
    de::EntityId b = {10, 1};
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    sim.world.get<de::Health>(b)->current = 5.0f;  // will die from 10 dmg
    // Prevent B from attacking back.
    sim.world.get<de::AttackCooldown>(b)->remaining = 100.0f;
    sim.world.get<de::AttackCooldown>(b)->interval = 100.0f;

    sim.tick(1.0);

    check(!sim.world.alive(b),
          "deferred_death: dead agent destroyed after tick");
    check(sim.world.entity_count() == 19,
          "deferred_death: entity count == 19");
    check(sim.world.alive(a),
          "deferred_death: attacker still alive");
}

// =================================================================
//  Snapshot combat metrics are coherent
// =================================================================

static void test_combat_snapshot_metrics() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};
    de::EntityId b = {10, 1};
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    sim.world.get<de::Health>(b)->current = 5.0f;
    sim.world.get<de::AttackCooldown>(b)->remaining = 100.0f;
    sim.world.get<de::AttackCooldown>(b)->interval = 100.0f;

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.attacks_this_tick >= 1,
          "combat_snapshot: at least 1 attack this tick");
    check(snap.deaths_this_tick >= 1,
          "combat_snapshot: at least 1 death this tick");
    check(snap.crowd_agent_count == 19,
          "combat_snapshot: 19 crowd agents after death");
}

// =================================================================
//  Full battle: enough ticks wipe out one team
// =================================================================

static void test_combat_full_battle() {
    de::SimState sim;
    sim.bootstrap_crowd();

    // Break symmetry: give team 0 a slight HP edge so it always wins.
    sim.world.each<de::CrowdAgent, de::Team, de::Health>(
        [](de::EntityId, de::CrowdAgent&, de::Team& t, de::Health& hp) {
            if (t.id == 0) hp.current = 105.0f;
        });

    // Run until one team is gone or max ticks.
    constexpr int max_ticks = 200;
    for (int i = 0; i < max_ticks; ++i) {
        sim.tick(1.0);
        de::SimSnapshot snap = sim.snapshot();
        if (snap.team_counts[0] == 0 || snap.team_counts[1] == 0) break;
    }

    de::SimSnapshot snap = sim.snapshot();
    bool one_team_wiped = (snap.team_counts[0] == 0 || snap.team_counts[1] == 0);
    check(one_team_wiped,
          "full_battle: one team is eliminated");
    check(snap.crowd_agent_count > 0,
          "full_battle: winners survive");
    check(snap.crowd_agent_count < 20,
          "full_battle: casualties occurred");
}

// =================================================================
//  agents_with_target excludes stale targets after death
// =================================================================

static void test_contract_target_metric_excludes_dead() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};
    de::EntityId b = {10, 1};
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    sim.world.get<de::Health>(b)->current = 5.0f;
    sim.world.get<de::AttackCooldown>(b)->remaining = 100.0f;
    sim.world.get<de::AttackCooldown>(b)->interval = 100.0f;

    sim.tick(1.0);

    // B is dead. A's Target still has has_target==true pointing at B.
    // The snapshot must NOT count A as "with target".
    de::SimSnapshot snap = sim.snapshot();
    check(!sim.world.alive(b),
          "target_metric: B is dead");

    // A still has has_target flag set, but target entity is dead.
    auto* tgt_a = sim.world.get<de::Target>(a);
    check(tgt_a && tgt_a->has_target,
          "target_metric: A still has stale has_target flag");

    // The metric must not count stale targets.
    // 18 surviving agents (excl A) should have valid targets pointing
    // at living enemies. A's target is stale => not counted.
    check(snap.agents_with_target <= snap.crowd_agent_count,
          "target_metric: agents_with_target <= crowd_agent_count");

    // Specifically: A has a stale target so should NOT be counted.
    // Count manually how many have truly valid targets.
    uint32_t truly_valid = 0;
    sim.world.each<de::CrowdAgent, de::Target>(
        [&](de::EntityId, de::CrowdAgent&, de::Target& t) {
            if (t.has_target && sim.world.alive(t.entity)) ++truly_valid;
        });
    check(snap.agents_with_target == truly_valid,
          "target_metric: snapshot matches ground truth");
}

// =================================================================
//  Simultaneous lethal exchange: both agents die in the same tick
// =================================================================

static void test_contract_simultaneous_lethal() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};
    de::EntityId b = {10, 1};
    // Place within attack range.
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.0f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    // Both have exactly enough HP to die from one hit.
    sim.world.get<de::Health>(a)->current = 10.0f;
    sim.world.get<de::Health>(b)->current = 10.0f;
    // damage = 10 (default), so each kills the other.

    sim.tick(1.0);

    // Simultaneous combat: both must die, regardless of iteration order.
    check(!sim.world.alive(a),
          "simultaneous_lethal: A dies");
    check(!sim.world.alive(b),
          "simultaneous_lethal: B dies");
    check(sim.world.entity_count() == 18,
          "simultaneous_lethal: 18 entities remain");
}

// =================================================================
//  Damage is order-independent: 3 attackers on same target
// =================================================================

static void test_contract_damage_order_independent() {
    de::SimState sim;
    sim.bootstrap_crowd();

    // Place 3 team-0 agents within range of 1 team-1 agent.
    de::EntityId a0 = {0, 1};
    de::EntityId a1 = {1, 1};
    de::EntityId a2 = {2, 1};
    de::EntityId b  = {10, 1};
    sim.world.get<de::Position>(a0)->x = 0.0f;
    sim.world.get<de::Position>(a0)->y = 0.0f;
    sim.world.get<de::Position>(a1)->x = 0.5f;
    sim.world.get<de::Position>(a1)->y = 0.0f;
    sim.world.get<de::Position>(a2)->x = -0.5f;
    sim.world.get<de::Position>(a2)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.0f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    // B won't attack back.
    sim.world.get<de::AttackCooldown>(b)->remaining = 100.0f;
    sim.world.get<de::AttackCooldown>(b)->interval = 100.0f;
    // damage = 10 each, so B takes 3 * 10 = 30.
    sim.world.get<de::Health>(b)->current = 100.0f;

    sim.tick(1.0);

    // Because damage is resolved simultaneously, B takes exactly 30.
    auto* hp_b = sim.world.get<de::Health>(b);
    check(hp_b && approx(hp_b->current, 70.0f),
          "order_independent: B HP == 70 (100 - 3*10)");
}

// =================================================================
//  Snapshot coherent after combat with deaths
// =================================================================

static void test_contract_snapshot_coherent_after_deaths() {
    de::SimState sim;
    sim.bootstrap_crowd();

    // Kill all of team 1 in one tick: set HP to 5, team 0 deals 10.
    // Keep y spread so each attacker targets a unique defender (1-to-1).
    sim.world.each<de::CrowdAgent, de::Team, de::Health, de::Position>(
        [](de::EntityId, de::CrowdAgent&, de::Team& t,
           de::Health& hp, de::Position& p) {
            if (t.id == 1) hp.current = 5.0f;
            p.x = (t.id == 0) ? 0.0f : 1.0f;
        });
    // Prevent team 1 from attacking back.
    sim.world.each<de::CrowdAgent, de::Team, de::AttackCooldown>(
        [](de::EntityId, de::CrowdAgent&, de::Team& t, de::AttackCooldown& cd) {
            if (t.id == 1) { cd.remaining = 100.0f; cd.interval = 100.0f; }
        });

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.crowd_agent_count == 10,
          "coherent_deaths: 10 agents survive");
    check(snap.team_counts[0] == 10,
          "coherent_deaths: team 0 intact");
    check(snap.team_counts[1] == 0,
          "coherent_deaths: team 1 wiped");
    check(snap.deaths_this_tick == 10,
          "coherent_deaths: 10 deaths this tick");
    check(snap.entity_count == 10,
          "coherent_deaths: entity_count == 10");
    // No surviving agent should have a valid target (all enemies dead).
    check(snap.agents_with_target == 0,
          "coherent_deaths: no valid targets after team wipe");
}

// =================================================================
//  Pre-dead agent does not select a target
// =================================================================

static void test_predead_no_target() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};   // team 0, will be pre-dead
    sim.world.get<de::Health>(a)->current = 0.0f;

    sim.tick(1.0);

    // A was culled before SelectTargets ran.
    check(!sim.world.alive(a),
          "predead_no_target: dead agent destroyed before systems");
    check(sim.world.entity_count() == 19,
          "predead_no_target: entity count == 19");
}

// =================================================================
//  Pre-dead agent does not move
// =================================================================

static void test_predead_no_move() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};
    sim.world.get<de::Health>(a)->current = -5.0f;
    sim.world.get<de::Velocity>(a)->dx = 99.0f;  // would move a lot

    sim.tick(1.0);

    // A is gone -- it never reached IntegratePosition.
    check(!sim.world.alive(a),
          "predead_no_move: dead agent destroyed before movement");
    // All other 19 agents are unaffected.
    check(sim.world.entity_count() == 19,
          "predead_no_move: 19 entities remain");
}

// =================================================================
//  Pre-dead agent does not produce attacks
// =================================================================

static void test_predead_no_attack() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};   // team 0, pre-dead
    de::EntityId b = {10, 1};  // team 1, potential victim
    // Place within attack range.
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.5f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    // A is already dead.
    sim.world.get<de::Health>(a)->current = 0.0f;
    // B does not attack back (isolate test).
    sim.world.get<de::AttackCooldown>(b)->remaining = 100.0f;
    sim.world.get<de::AttackCooldown>(b)->interval = 100.0f;

    sim.tick(1.0);

    check(!sim.world.alive(a),
          "predead_no_attack: dead agent destroyed");
    auto* hp_b = sim.world.get<de::Health>(b);
    check(hp_b && approx(hp_b->current, 100.0f),
          "predead_no_attack: victim HP unchanged (dead agent did not attack)");
}

// =================================================================
//  Agent alive at tick start can still act even if killed this tick
// =================================================================

static void test_alive_at_start_acts_before_death() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::EntityId a = {0, 1};   // team 0
    de::EntityId b = {10, 1};  // team 1
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 1.0f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    // Both have 10 HP, both deal 10 damage => mutual kill.
    sim.world.get<de::Health>(a)->current = 10.0f;
    sim.world.get<de::Health>(b)->current = 10.0f;

    sim.tick(1.0);

    // Both were alive at tick start (HP > 0) => both acted.
    // Simultaneous damage: both took 10 damage => HP 0 => both dead.
    check(!sim.world.alive(a),
          "alive_acts: A died (killed by B this tick)");
    check(!sim.world.alive(b),
          "alive_acts: B died (killed by A this tick)");

    // Key assertion: both attacked, proving A acted even though B killed it.
    de::SimSnapshot snap = sim.snapshot();
    check(snap.attacks_this_tick >= 2,
          "alive_acts: both agents attacked (simultaneous contract)");
}

// =================================================================
//  Grid: existing small-scene targeting unchanged
// =================================================================

static void test_grid_small_scene_regression() {
    de::SimState sim;
    sim.bootstrap_crowd();
    sim.tick(1.0);

    // Same checks as test_crowd_target_selection + test_crowd_target_is_enemy.
    uint32_t with_target = 0;
    bool all_enemy = true;
    sim.world.each<de::CrowdAgent, de::Team, de::Target>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& my_team, de::Target& tgt) {
            if (tgt.has_target) {
                ++with_target;
                auto* et = sim.world.get<de::Team>(tgt.entity);
                if (!et || et->id == my_team.id) all_enemy = false;
            }
        });

    check(with_target == 20,
          "grid_regression: all 20 agents have target");
    check(all_enemy,
          "grid_regression: all targets are enemies");
}

// =================================================================
//  Grid: controlled scene -- nearest enemy is correct
// =================================================================

static void test_grid_controlled_nearest() {
    de::SimState sim;
    sim.bootstrap_crowd();

    // Move agent {0,1} (team 0) to origin.
    // Move agent {10,1} (team 1) to (5, 0).
    // Move agent {11,1} (team 1) to (100, 0) -- far away.
    de::EntityId a  = {0, 1};
    de::EntityId b  = {10, 1};
    de::EntityId c  = {11, 1};
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 5.0f;
    sim.world.get<de::Position>(b)->y = 0.0f;
    sim.world.get<de::Position>(c)->x = 100.0f;
    sim.world.get<de::Position>(c)->y = 0.0f;

    sim.tick(1.0);

    // A should target B (closer) not C (farther).
    auto* tgt = sim.world.get<de::Target>(a);
    check(tgt && tgt->has_target && tgt->entity == b,
          "grid_nearest: agent targets nearest enemy via grid");
}

// =================================================================
//  Grid: large scene (2x100) correct and grid is faster
// =================================================================

static void test_grid_large_scene() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 100;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    check(sim.world.entity_count() == 200,
          "grid_large: 200 agents created");

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.crowd_agent_count == 200,
          "grid_large: 200 crowd agents after tick");
    check(snap.agents_with_target == 200,
          "grid_large: all 200 have targets");

    // All targets must be enemies.
    bool all_enemy = true;
    sim.world.each<de::CrowdAgent, de::Team, de::Target>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& my_team, de::Target& tgt) {
            if (!tgt.has_target) { all_enemy = false; return; }
            auto* et = sim.world.get<de::Team>(tgt.entity);
            if (!et || et->id == my_team.id) all_enemy = false;
        });
    check(all_enemy, "grid_large: all targets are enemies");

    // Key assertion: the grid scanned fewer candidates than brute-force.
    // Brute-force would check 200 * 100 = 20000 enemy comparisons
    // (each of 200 agents checks ~100 enemies in the inner loop).
    uint32_t brute_force = 200 * 100;
    check(snap.targeting_candidates_scanned < brute_force,
          "grid_large: candidates scanned < brute-force total");
}

// =================================================================
//  Grid: huge spacing -- enemy beyond old 200-ring cap
// =================================================================

static void test_grid_huge_spacing_finds_enemy() {
    // team_spacing = 5000 => teams at x=-5000 and x=+5000
    // With cell_size=10, that is 1000 cells apart -- well past the old
    // ring<=200 hard cap.
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 5000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.crowd_agent_count == 2,
          "grid_huge: 2 agents alive");
    check(snap.agents_with_target == 2,
          "grid_huge: both agents have target (no false negative)");
}

// =================================================================
//  Grid: unit-level API -- SpatialGrid alone, huge distance
// =================================================================

static void test_grid_huge_spacing_unit_api() {
    de::SpatialGrid grid;
    grid.cell_size = 10.0f;

    de::EntityId a = {0, 1};
    de::EntityId b = {1, 1};

    // Place two enemies 50000 units apart (5000 cells).
    grid.insert(a, 0, -25000.0f, 0.0f);
    grid.insert(b, 1,  25000.0f, 0.0f);

    uint32_t checked = 0;
    auto r = grid.find_nearest_enemy(-25000.0f, 0.0f, 0, a, checked);
    check(r.found,
          "grid_unit_huge: found enemy at 50000 units");
    check(r.id == b,
          "grid_unit_huge: correct enemy id");
    check(checked == 1,
          "grid_unit_huge: exactly 1 candidate checked");
}

// =================================================================
//  Separation: two close agents push apart
// =================================================================

static void test_separation_two_close_agents() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 0.2f;   // teams 0.4 units apart (< default radius 0.8)
    cfg.separation_radius   = 0.8f;
    cfg.separation_strength = 5.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Record initial distance.
    de::EntityId a = {0, 1};
    de::EntityId b = {1, 1};
    float ax0 = sim.world.get<de::Position>(a)->x;
    float bx0 = sim.world.get<de::Position>(b)->x;
    float dist0 = std::abs(bx0 - ax0);

    sim.tick(0.1);

    float ax1 = sim.world.get<de::Position>(a)->x;
    float bx1 = sim.world.get<de::Position>(b)->x;
    float dist1 = std::abs(bx1 - ax1);

    check(dist1 > dist0,
          "sep_two_close: agents pushed apart after 1 tick");
}

// =================================================================
//  Separation: dense group does not collapse to a single point
// =================================================================

static void test_separation_dense_no_collapse() {
    de::CrowdConfig cfg;
    cfg.agents_per_team     = 5;
    cfg.team_spacing        = 50.0f;  // far enough that pursuit takes time
    cfg.agent_spread        = 0.1f;   // very tight initial packing along y
    cfg.health              = 10000.0f;
    cfg.separation_radius   = 0.8f;
    cfg.separation_strength = 5.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Run a few ticks for separation to spread agents along y.
    for (int i = 0; i < 10; ++i) sim.tick(0.1);

    // Check that same-team agents spread out along y (initial spread was 0.4).
    float min_y = 1e9f, max_y = -1e9f;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id != 0) return;
            if (p.y < min_y) min_y = p.y;
            if (p.y > max_y) max_y = p.y;
        });
    float spread = max_y - min_y;
    check(spread > 0.5f,
          "sep_dense: team 0 did not collapse to a single point");
}

// =================================================================
//  Separation: distant agents are unaffected
// =================================================================

static void test_separation_distant_unaffected() {
    de::CrowdConfig cfg;
    cfg.agents_per_team    = 1;
    cfg.team_spacing       = 100.0f;   // far apart
    cfg.separation_radius  = 0.8f;
    cfg.separation_strength = 5.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    sim.tick(0.1);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.separation_pairs_this_tick == 0,
          "sep_distant: zero separation pairs when agents far apart");
}

// =================================================================
//  Separation: combat still works at close range
// =================================================================

static void test_separation_combat_still_works() {
    de::CrowdConfig cfg;
    cfg.agents_per_team    = 1;
    cfg.team_spacing       = 0.5f;   // within attack range (2.0)
    cfg.health             = 100.0f;
    cfg.attack_damage      = 10.0f;
    cfg.attack_interval    = 0.5f;
    cfg.separation_radius  = 0.8f;
    cfg.separation_strength = 5.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Run enough ticks for at least one attack to land.
    for (int i = 0; i < 5; ++i) sim.tick(0.5);

    // At least one agent should have taken damage.
    bool someone_hit = false;
    sim.world.each<de::CrowdAgent, de::Health>(
        [&](de::EntityId, de::CrowdAgent&, de::Health& hp) {
            if (hp.current < hp.max) someone_hit = true;
        });
    check(someone_hit,
          "sep_combat: separation does not prevent combat");
}

// =================================================================
//  Separation: telemetry is coherent
// =================================================================

static void test_separation_telemetry() {
    de::CrowdConfig cfg;
    cfg.agents_per_team    = 5;
    cfg.team_spacing       = 1.0f;    // close enough for overlap
    cfg.agent_spread       = 0.1f;
    cfg.separation_radius  = 0.8f;
    cfg.separation_strength = 5.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(0.1);

    de::SimSnapshot snap = sim.snapshot();
    // With 10 agents packed tightly, separation_pairs should be > 0.
    check(snap.separation_pairs_this_tick > 0,
          "sep_telemetry: separation pairs counted");
    // Each pair is counted once per querying agent, so pairs should be
    // at most N * (N-1) for N agents in overlap range.
    uint32_t max_pairs = 10 * 9;
    check(snap.separation_pairs_this_tick <= max_pairs,
          "sep_telemetry: pairs within upper bound");
}

// =================================================================
//  Separation: exact overlap -- agents at same point separate
// =================================================================

static void test_separation_exact_overlap() {
    de::CrowdConfig cfg;
    cfg.agents_per_team     = 1;
    cfg.team_spacing        = 0.0f;   // both teams at origin
    cfg.separation_radius   = 0.8f;
    cfg.separation_strength = 5.0f;
    cfg.health              = 10000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    de::EntityId a = {0, 1};
    de::EntityId b = {1, 1};

    // Force exact overlap.
    sim.world.get<de::Position>(a)->x = 0.0f;
    sim.world.get<de::Position>(a)->y = 0.0f;
    sim.world.get<de::Position>(b)->x = 0.0f;
    sim.world.get<de::Position>(b)->y = 0.0f;

    sim.tick(0.1);

    float ax = sim.world.get<de::Position>(a)->x;
    float bx = sim.world.get<de::Position>(b)->x;
    float dist = std::abs(ax - bx);
    check(dist > 1e-6f,
          "sep_exact: agents at same point separated after 1 tick");
}

// =================================================================
//  Separation: exact overlap midpoint does not drift
// =================================================================

static void test_separation_exact_overlap_midpoint() {
    de::CrowdConfig cfg;
    cfg.agents_per_team     = 1;
    cfg.team_spacing        = 0.0f;
    cfg.separation_radius   = 0.8f;
    cfg.separation_strength = 5.0f;
    cfg.health              = 10000.0f;
    cfg.move_speed          = 100.0f;  // high so clamp doesn't zero separation

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    de::EntityId a = {0, 1};
    de::EntityId b = {1, 1};

    sim.world.get<de::Position>(a)->x = 10.0f;
    sim.world.get<de::Position>(a)->y = 5.0f;
    sim.world.get<de::Position>(b)->x = 10.0f;
    sim.world.get<de::Position>(b)->y = 5.0f;

    // Disable pursuit so only separation acts on position.
    sim.world.get<de::Target>(a)->has_target = false;
    sim.world.get<de::Target>(b)->has_target = false;

    sim.tick(0.1);

    float ax = sim.world.get<de::Position>(a)->x;
    float bx = sim.world.get<de::Position>(b)->x;
    float midpoint = (ax + bx) * 0.5f;
    check(std::abs(midpoint - 10.0f) < 1e-4f,
          "sep_midpoint: midpoint did not drift on exact overlap");
}

// =================================================================
//  Separation: speed clamp -- velocity never exceeds MoveSpeed.max
// =================================================================

static void test_separation_speed_clamp() {
    de::CrowdConfig cfg;
    cfg.agents_per_team     = 10;
    cfg.team_spacing        = 0.5f;
    cfg.agent_spread        = 0.05f;  // extreme packing
    cfg.move_speed          = 3.0f;
    cfg.separation_radius   = 0.8f;
    cfg.separation_strength = 50.0f;  // very strong to stress clamp
    cfg.health              = 10000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(0.1);

    bool all_clamped = true;
    sim.world.each<de::CrowdAgent, de::Velocity, de::MoveSpeed>(
        [&](de::EntityId, de::CrowdAgent&, de::Velocity& vel, de::MoveSpeed& spd) {
            float speed = std::sqrt(vel.dx * vel.dx + vel.dy * vel.dy);
            if (speed > spd.max + 1e-4f) all_clamped = false;
        });
    check(all_clamped,
          "sep_clamp: velocity never exceeds MoveSpeed.max");
}

// =================================================================
//  Battle goal: agents follow goal when no enemy is close
// =================================================================

static void test_goal_follow_without_enemy() {
    // Large engage_radius but huge spacing so enemies are far away.
    de::CrowdConfig cfg;
    cfg.agents_per_team = 3;
    cfg.team_spacing    = 500.0f;     // enemies 1000 units apart
    cfg.engage_radius   = 15.0f;     // way below 1000
    cfg.health          = 10000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Team 0 starts at x=-500, goal at x=+500.
    float x0_before = sim.world.get<de::Position>(de::EntityId{0,1})->x;
    sim.tick(1.0);
    float x0_after  = sim.world.get<de::Position>(de::EntityId{0,1})->x;

    check(x0_after > x0_before,
          "goal_follow: team 0 moved toward goal (+X)");

    // Team 1 starts at x=+500, goal at x=-500.
    float x1_before_approx = 500.0f;  // initial spawn
    float x1_after = sim.world.get<de::Position>(de::EntityId{3,1})->x;
    check(x1_after < x1_before_approx,
          "goal_follow: team 1 moved toward goal (-X)");

    // No agent should be engaged (enemies too far).
    de::SimSnapshot snap = sim.snapshot();
    check(snap.agents_engaged == 0,
          "goal_follow: zero agents engaged when enemies far");
}

// =================================================================
//  Battle goal: two teams converge toward each other
// =================================================================

static void test_goal_convergence() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 100.0f;
    cfg.engage_radius   = 15.0f;
    cfg.health          = 10000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Run several ticks.
    for (int i = 0; i < 10; ++i) sim.tick(1.0);

    // Compute average x per team -- they should have moved closer.
    float sum_t0 = 0.0f, sum_t1 = 0.0f;
    int n0 = 0, n1 = 0;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0) { sum_t0 += p.x; ++n0; }
            else           { sum_t1 += p.x; ++n1; }
        });
    float avg0 = sum_t0 / static_cast<float>(n0);
    float avg1 = sum_t1 / static_cast<float>(n1);

    check(avg0 > -100.0f, "goal_converge: team 0 advanced from spawn");
    check(avg1 <  100.0f, "goal_converge: team 1 advanced from spawn");
    check(avg0 < avg1,    "goal_converge: teams moved toward each other");
}

// =================================================================
//  Battle goal: engagement overrides goal when enemy is close
// =================================================================

static void test_goal_engage_overrides() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 5.0f;   // enemies 10 units apart
    cfg.engage_radius   = 15.0f;  // enemies within range
    cfg.health          = 10000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.agents_engaged == 2,
          "goal_engage: both agents engaged when close");

    // Agent should be moving toward enemy, not toward goal.
    // Team 0 at x=-5, enemy at x=+5, goal at x=+5 (same direction here).
    // Team 1 at x=+5, enemy at x=-5, goal at x=-5 (same direction too).
    // Better test: verify pursuit direction is toward the specific enemy.
    auto* vel0 = sim.world.get<de::Velocity>(de::EntityId{0,1});
    check(vel0 && vel0->dx > 0.0f,
          "goal_engage: team 0 pursues enemy in +X");
}

// =================================================================
//  Battle goal: separation still works alongside goals
// =================================================================

static void test_goal_separation_compat() {
    de::CrowdConfig cfg;
    cfg.agents_per_team     = 5;
    cfg.team_spacing        = 200.0f;
    cfg.agent_spread        = 0.1f;
    cfg.separation_radius   = 0.8f;
    cfg.separation_strength = 5.0f;
    cfg.engage_radius       = 15.0f;
    cfg.health              = 10000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    for (int i = 0; i < 10; ++i) sim.tick(0.1);

    // Same-team agents should have spread apart (separation works).
    float min_y = 1e9f, max_y = -1e9f;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id != 0) return;
            if (p.y < min_y) min_y = p.y;
            if (p.y > max_y) max_y = p.y;
        });
    check((max_y - min_y) > 0.5f,
          "goal_sep: separation still spreads agents with battle goal");
}

// =================================================================
//  Battle goal: snapshot telemetry is coherent
// =================================================================

static void test_goal_snapshot_coherent() {
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 100.0f;
    cfg.engage_radius   = 15.0f;
    cfg.health          = 10000.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    // Enemies are 200 units apart, engage_radius=15 => nobody engaged.
    check(snap.agents_engaged == 0,
          "goal_snap: no engagement when teams far apart");
    check(snap.crowd_agent_count == 10,
          "goal_snap: all agents alive");
    check(snap.agents_with_target == 10,
          "goal_snap: all agents have targeting info");
}

// =================================================================
//  main
// =================================================================

int main() {
    test_crowd_bootstrap_count();
    test_crowd_team_distribution();
    test_crowd_components_present();
    test_crowd_initial_positions();
    test_crowd_target_selection();
    test_crowd_target_is_enemy();
    test_crowd_desired_direction();
    test_crowd_convergence();
    test_crowd_system_order();
    test_crowd_snapshot_metrics();
    test_crowd_speed_clamp();
    test_crowd_bootstrap_idempotent();

    // Combat tests
    test_combat_stop_in_range();
    test_combat_attack_reduces_hp();
    test_combat_cooldown();
    test_combat_deferred_death();
    test_combat_snapshot_metrics();
    test_combat_full_battle();

    // Contract tests
    test_contract_target_metric_excludes_dead();
    test_contract_simultaneous_lethal();
    test_contract_damage_order_independent();
    test_contract_snapshot_coherent_after_deaths();

    // Alive-at-tick-start contract tests
    test_predead_no_target();
    test_predead_no_move();
    test_predead_no_attack();
    test_alive_at_start_acts_before_death();

    // Spatial grid tests
    test_grid_small_scene_regression();
    test_grid_controlled_nearest();
    test_grid_large_scene();
    test_grid_huge_spacing_finds_enemy();
    test_grid_huge_spacing_unit_api();

    // Separation tests
    test_separation_two_close_agents();
    test_separation_dense_no_collapse();
    test_separation_distant_unaffected();
    test_separation_combat_still_works();
    test_separation_telemetry();
    test_separation_exact_overlap();
    test_separation_exact_overlap_midpoint();
    test_separation_speed_clamp();

    // Battle goal tests
    test_goal_follow_without_enemy();
    test_goal_convergence();
    test_goal_engage_overrides();
    test_goal_separation_compat();
    test_goal_snapshot_coherent();

    std::printf("\nCrowdTest results: %d passed, %d failed\n",
                g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
