#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"

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
    check(snap.system_count == 5, "system_order: 5 systems registered");

    check(std::strcmp(snap.systems[0].name, "SelectTargets") == 0,
          "system_order: [0] SelectTargets");
    check(std::strcmp(snap.systems[1].name, "ComputeDesiredMove") == 0,
          "system_order: [1] ComputeDesiredMove");
    check(std::strcmp(snap.systems[2].name, "ApplyCrowdSteer") == 0,
          "system_order: [2] ApplyCrowdSteer");
    check(std::strcmp(snap.systems[3].name, "IntegrateVelocity") == 0,
          "system_order: [3] IntegrateVelocity");
    check(std::strcmp(snap.systems[4].name, "IntegratePosition") == 0,
          "system_order: [4] IntegratePosition");
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
    check(snap.system_count == 5,
          "crowd_idempotent: 5 systems, not 10");
    check(snap.crowd_agent_count == 20,
          "crowd_idempotent: crowd metrics correct after re-bootstrap");
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

    std::printf("\nCrowdTest results: %d passed, %d failed\n",
                g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
