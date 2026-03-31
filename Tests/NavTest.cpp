#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"
#include "Runtime/BattlefieldGrid.h"

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

static bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabsf(a - b) < eps;
}

// =================================================================
//  BattlefieldGrid: basic BFS on empty grid
// =================================================================

static void test_grid_empty_field() {
    de::BattlefieldGrid grid;
    grid.init(10, 10, 1.0f, 0.0f, 0.0f);

    uint32_t reachable = grid.build_integration_field(9.5f, 5.5f);
    check(reachable == 100, "grid_empty: all 100 cells reachable");

    // Cell at goal should have cost 0.
    check(approx(grid.cost_at(9.5f, 5.5f), 0.0f),
          "grid_empty: goal cell cost == 0");

    // Cell at origin (0,0) -> cell (0,0) should have cost == manhattan to (9,5).
    float c = grid.cost_at(0.5f, 0.5f);
    check(c > 0.0f && c <= 14.0f,
          "grid_empty: origin cell has reasonable cost");
}

// =================================================================
//  BattlefieldGrid: blocked cells are unreachable
// =================================================================

static void test_grid_blocked_cell() {
    de::BattlefieldGrid grid;
    grid.init(5, 5, 1.0f, 0.0f, 0.0f);

    // Block center cell.
    grid.set_blocked(2, 2);
    grid.build_integration_field(4.5f, 2.5f);

    check(grid.cost_at(2.5f, 2.5f) >= de::BattlefieldGrid::k_unreachable,
          "grid_blocked: blocked cell cost is unreachable");
    check(grid.cost_at(0.5f, 2.5f) < de::BattlefieldGrid::k_unreachable,
          "grid_blocked: non-blocked cell is reachable");
}

// =================================================================
//  BattlefieldGrid: flow direction points toward goal
// =================================================================

static void test_grid_flow_direction() {
    de::BattlefieldGrid grid;
    grid.init(10, 1, 1.0f, 0.0f, 0.0f);

    // 1D corridor, goal at right end.
    grid.build_integration_field(9.5f, 0.5f);

    float dx, dy;
    bool ok = grid.sample_flow(0.5f, 0.5f, dx, dy);
    check(ok, "grid_flow_1d: sample succeeded");
    check(dx > 0.0f, "grid_flow_1d: flow points toward +x (goal)");
}

// =================================================================
//  BattlefieldGrid: wall with gap routes around
// =================================================================

static void test_grid_wall_with_gap() {
    // 20x10 grid.  Vertical wall at x=10, gap at y=5.
    de::BattlefieldGrid grid;
    grid.init(20, 10, 1.0f, 0.0f, 0.0f);

    // Block column x=10 except y=5.
    for (int y = 0; y < 10; ++y) {
        if (y != 5) grid.set_blocked(10, y);
    }

    // Goal at right side.
    uint32_t reachable = grid.build_integration_field(19.5f, 5.5f);
    check(reachable > 0, "grid_wall_gap: some cells reachable");

    // Cell just left of wall at y=0 should route through the gap.
    // Cost should be > direct manhattan (9 cells horizontal) because
    // it has to detour through the gap.
    float cost_near_wall = grid.cost_at(9.5f, 0.5f);
    float cost_at_gap    = grid.cost_at(9.5f, 5.5f);
    check(cost_near_wall > cost_at_gap,
          "grid_wall_gap: cost near wall > cost at gap level");

    // Flow at (9, 0) should NOT point right (blocked) but toward gap.
    float dx, dy;
    grid.sample_flow(9.5f, 0.5f, dx, dy);
    check(dy > 0.0f || dx < 0.0f,
          "grid_wall_gap: flow near wall does not point into wall");
}

// =================================================================
//  Regression: no-obstacle crowd behaves same as before
// =================================================================

static void test_nav_regression_no_obstacle() {
    // Scene without navigation grid (classic behavior).
    de::SimState sim_classic;
    sim_classic.bootstrap_crowd();
    for (int i = 0; i < 5; ++i) sim_classic.tick(1.0);

    // Scene with navigation grid but zero obstacles.
    de::BattlefieldConfig bcfg;
    bcfg.crowd.agents_per_team = 10;
    bcfg.obstacle_count = 0;

    de::SimState sim_nav;
    sim_nav.bootstrap_battlefield(bcfg);
    for (int i = 0; i < 5; ++i) sim_nav.tick(1.0);

    // Both scenes should converge similarly: gap decreases.
    float max_x_team0_classic = -1e6f;
    float min_x_team1_classic =  1e6f;
    sim_classic.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0 && p.x > max_x_team0_classic) max_x_team0_classic = p.x;
            if (t.id == 1 && p.x < min_x_team1_classic) min_x_team1_classic = p.x;
        });

    float max_x_team0_nav = -1e6f;
    float min_x_team1_nav =  1e6f;
    sim_nav.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0 && p.x > max_x_team0_nav) max_x_team0_nav = p.x;
            if (t.id == 1 && p.x < min_x_team1_nav) min_x_team1_nav = p.x;
        });

    float gap_classic = min_x_team1_classic - max_x_team0_classic;
    float gap_nav     = min_x_team1_nav     - max_x_team0_nav;

    check(gap_classic < 15.0f,
          "nav_regression: classic gap < 15 after 5 ticks");
    check(gap_nav < 15.0f,
          "nav_regression: nav gap < 15 after 5 ticks");
    // The nav version should converge to roughly the same distance.
    check(std::fabsf(gap_classic - gap_nav) < 5.0f,
          "nav_regression: classic and nav gaps are similar (within 5 units)");
}

// =================================================================
//  Wall scene: agents avoid the wall and no agent sits in a blocked cell
// =================================================================

static void test_nav_wall_agents_avoid() {
    // Grid: 60 wide, 40 tall, cell=1, origin=(-30, -20).
    // Vertical wall at column x_cell=30 (world x=0..1), gap at row y_cell=20 (world y=0..1).
    de::ObstacleDef obstacles[39];
    int count = 0;
    for (int y = 0; y < 40; ++y) {
        if (y != 20) {
            obstacles[count++] = {30, y};
        }
    }

    de::BattlefieldConfig bcfg;
    bcfg.crowd.agents_per_team = 5;
    bcfg.crowd.team_spacing    = 20.0f;
    bcfg.crowd.agent_spread    = 1.0f;
    bcfg.crowd.move_speed      = 3.0f;
    bcfg.crowd.engage_radius   = 3.0f;   // small so nav dominates
    bcfg.crowd.attack_range    = 1.5f;
    bcfg.crowd.health          = 10000.0f;  // prevent deaths
    bcfg.grid_width            = 60;
    bcfg.grid_height           = 40;
    bcfg.grid_cell             = 1.0f;
    bcfg.grid_ox               = -30.0f;
    bcfg.grid_oy               = -20.0f;
    bcfg.obstacles             = obstacles;
    bcfg.obstacle_count        = count;

    de::SimState sim;
    sim.bootstrap_battlefield(bcfg);

    // Run enough ticks for agents to reach the wall area.
    for (int i = 0; i < 10; ++i) {
        sim.tick(1.0);
    }

    // 1. Agents advanced from start.
    float max_x_team0 = -1e6f;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0 && p.x > max_x_team0) max_x_team0 = p.x;
        });
    check(max_x_team0 > -20.0f,
          "nav_wall: team 0 agents advanced from start");

    // 2. No agent occupies a blocked cell (the wall) except via the gap.
    //    The wall is at cell column 30 (world x in [0, 1]).
    //    The gap is at cell row 20 (world y in [0, 1]).
    bool none_in_wall = true;
    sim.world.each<de::CrowdAgent, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Position& p) {
            int cx, cy;
            de::BattlefieldGrid probe;
            probe.init(bcfg.grid_width, bcfg.grid_height,
                       bcfg.grid_cell, bcfg.grid_ox, bcfg.grid_oy);
            probe.world_to_cell(p.x, p.y, cx, cy);
            // Check if this cell is a blocked wall cell (col 30, row != 20).
            if (cx == 30 && cy != 20) {
                none_in_wall = false;
            }
        });
    check(none_in_wall,
          "nav_wall: no agent occupies a blocked wall cell");

    // 3. Telemetry coherent.
    de::SimSnapshot snap = sim.snapshot();
    check(snap.nav_queries_this_tick > 0,
          "nav_wall: navigation queries occurred");
    check(snap.nav_blocked_cells == 39,
          "nav_wall: 39 blocked cells in snapshot");
}

// =================================================================
//  Wall scene: agents eventually reach the other side through gap
// =================================================================

static void test_nav_wall_agents_cross_gap() {
    de::ObstacleDef obstacles[39];
    int count = 0;
    for (int y = 0; y < 40; ++y) {
        if (y != 20) {
            obstacles[count++] = {30, y};
        }
    }

    de::BattlefieldConfig bcfg;
    bcfg.crowd.agents_per_team = 3;
    bcfg.crowd.team_spacing    = 10.0f;
    bcfg.crowd.agent_spread    = 0.5f;
    bcfg.crowd.move_speed      = 3.0f;
    bcfg.crowd.health          = 10000.0f;  // prevent deaths
    bcfg.crowd.engage_radius   = 2.0f;      // very small
    bcfg.crowd.attack_range    = 1.0f;
    bcfg.grid_width            = 60;
    bcfg.grid_height           = 40;
    bcfg.grid_cell             = 1.0f;
    bcfg.grid_ox               = -30.0f;
    bcfg.grid_oy               = -20.0f;
    bcfg.obstacles             = obstacles;
    bcfg.obstacle_count        = count;

    de::SimState sim;
    sim.bootstrap_battlefield(bcfg);

    // Run enough ticks for agents to reach the other side.
    // Distance ~20 units, speed 3 u/s, but detour through gap adds ~20.
    // So ~40/3 ~= 14 ticks minimum.  Give plenty of margin.
    for (int i = 0; i < 40; ++i) {
        sim.tick(1.0);
    }

    // At least one team-0 agent should have crossed x=0 (the wall).
    bool team0_crossed = false;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0 && p.x > 1.0f) team0_crossed = true;
        });

    check(team0_crossed,
          "nav_cross_gap: at least one team-0 agent crossed the wall");

    // Similarly for team 1.
    bool team1_crossed = false;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 1 && p.x < -1.0f) team1_crossed = true;
        });

    check(team1_crossed,
          "nav_cross_gap: at least one team-1 agent crossed the wall");
}

// =================================================================
//  Engage radius still overrides flow-field navigation
// =================================================================

static void test_nav_engage_overrides_flow() {
    // Empty grid (no obstacles), but with navigation active.
    de::BattlefieldConfig bcfg;
    bcfg.crowd.agents_per_team = 1;
    bcfg.crowd.team_spacing    = 5.0f;
    bcfg.crowd.engage_radius   = 20.0f;   // very large -> always engage
    bcfg.crowd.attack_range    = 1.5f;
    bcfg.obstacle_count        = 0;

    de::SimState sim;
    sim.bootstrap_battlefield(bcfg);

    // After 1 tick, agents should be pursuing each other (engage kicks in).
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.agents_engaged > 0,
          "nav_engage: agents still engage when enemy is within radius");

    // Desired direction for team 0 should be toward the enemy (+x)
    // regardless of what the flow field says.
    bool dir_ok = true;
    sim.world.each<de::CrowdAgent, de::Team, de::DesiredDirection>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::DesiredDirection& d) {
            // Team 0 at x=-5, enemy at x=+5 -> should pursue toward +x.
            // Team 1 at x=+5, enemy at x=-5 -> should pursue toward -x.
            // (engage overrides flow field)
            if (t.id == 0 && d.dx <= 0.0f) dir_ok = false;
            if (t.id == 1 && d.dx >= 0.0f) dir_ok = false;
        });

    check(dir_ok, "nav_engage: tactical pursuit overrides flow field direction");
}

// =================================================================
//  Agent out of grid gets zero direction (no silent bypass)
// =================================================================

static void test_nav_out_of_grid_no_bypass() {
    // Small grid covering only [-5, +5] in both axes.
    de::BattlefieldConfig bcfg;
    bcfg.crowd.agents_per_team = 1;
    bcfg.crowd.team_spacing    = 3.0f;    // agents at x=-3 and x=+3
    bcfg.crowd.engage_radius   = 1.0f;    // tiny so nav dominates
    bcfg.grid_width            = 10;
    bcfg.grid_height           = 10;
    bcfg.grid_cell             = 1.0f;
    bcfg.grid_ox               = -5.0f;
    bcfg.grid_oy               = -5.0f;
    bcfg.obstacle_count        = 0;

    de::SimState sim;
    sim.bootstrap_battlefield(bcfg);

    // Move team 0 agent outside the grid.
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0) { p.x = -50.0f; p.y = 0.0f; }
        });

    sim.tick(1.0);

    // The out-of-grid agent should have zero velocity (no bypass).
    // It must NOT have moved toward the goal via direct line.
    bool zero_vel = true;
    sim.world.each<de::CrowdAgent, de::Team, de::Velocity>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Velocity& v) {
            if (t.id == 0) {
                // Velocity should be ~0 because direction was zeroed.
                // (Separation might add a tiny nudge, so allow small epsilon.)
                float speed = std::sqrt(v.dx * v.dx + v.dy * v.dy);
                if (speed > 0.5f) zero_vel = false;
            }
        });
    check(zero_vel,
          "nav_out_of_grid: agent outside grid has near-zero velocity");

    // The failure counter should have incremented.
    de::SimSnapshot snap = sim.snapshot();
    check(snap.nav_failures_this_tick > 0,
          "nav_out_of_grid: nav failure counted for out-of-grid agent");
}

// =================================================================
//  Wall scene: agents cross through gap, not through wall
// =================================================================

static void test_nav_wall_no_crossing_through_wall() {
    // Track agent positions every tick and verify no agent teleports
    // through the wall (crosses x=0 outside the gap y-band).
    de::ObstacleDef obstacles[39];
    int count = 0;
    for (int y = 0; y < 40; ++y) {
        if (y != 20) {
            obstacles[count++] = {30, y};
        }
    }

    de::BattlefieldConfig bcfg;
    bcfg.crowd.agents_per_team = 3;
    bcfg.crowd.team_spacing    = 10.0f;
    bcfg.crowd.agent_spread    = 0.5f;
    bcfg.crowd.move_speed      = 3.0f;
    bcfg.crowd.health          = 10000.0f;
    bcfg.crowd.engage_radius   = 2.0f;
    bcfg.crowd.attack_range    = 1.0f;
    bcfg.grid_width            = 60;
    bcfg.grid_height           = 40;
    bcfg.grid_cell             = 1.0f;
    bcfg.grid_ox               = -30.0f;
    bcfg.grid_oy               = -20.0f;
    bcfg.obstacles             = obstacles;
    bcfg.obstacle_count        = count;

    de::SimState sim;
    sim.bootstrap_battlefield(bcfg);

    // Track: did any team-0 agent ever occupy a blocked wall cell?
    bool wall_violated = false;
    for (int tick = 0; tick < 40; ++tick) {
        sim.tick(1.0);
        sim.world.each<de::CrowdAgent, de::Position>(
            [&](de::EntityId, de::CrowdAgent&, de::Position& p) {
                // Wall: cell column 30 = world x in [0, 1], all rows except 20.
                int cx = static_cast<int>(std::floor((p.x - bcfg.grid_ox) / bcfg.grid_cell));
                int cy = static_cast<int>(std::floor((p.y - bcfg.grid_oy) / bcfg.grid_cell));
                if (cx == 30 && cy != 20) {
                    wall_violated = true;
                }
            });
    }
    check(!wall_violated,
          "nav_wall_integrity: no agent passed through a blocked wall cell");
}

// =================================================================
//  Invalid battlefield config falls back gracefully
// =================================================================

static void test_nav_invalid_config_fallback() {
    // Zero-width grid should not crash; should fall back to plain crowd.
    de::BattlefieldConfig bcfg;
    bcfg.grid_width  = 0;
    bcfg.grid_height = 0;
    bcfg.crowd.agents_per_team = 2;

    de::SimState sim;
    sim.bootstrap_battlefield(bcfg);

    // Should have agents but no nav grid.
    check(sim.world.entity_count() == 4,
          "nav_invalid_config: 4 agents created despite invalid grid");

    sim.tick(1.0);

    // No nav queries (grid not installed).
    de::SimSnapshot snap = sim.snapshot();
    check(snap.nav_queries_this_tick == 0,
          "nav_invalid_config: no nav queries with invalid grid");
    check(snap.nav_failures_this_tick == 0,
          "nav_invalid_config: no nav failures with invalid grid");

    // Agents still move (direct line fallback, no grid installed).
    bool moved = false;
    sim.world.each<de::CrowdAgent, de::Team, de::Position>(
        [&](de::EntityId, de::CrowdAgent&, de::Team& t, de::Position& p) {
            if (t.id == 0 && p.x > -20.0f + 0.1f) moved = true;
        });
    check(moved,
          "nav_invalid_config: agents move via direct line (no grid)");
}

// =================================================================
//  Telemetry: nav_queries > 0 with grid active
// =================================================================

static void test_nav_telemetry() {
    de::BattlefieldConfig bcfg;
    bcfg.crowd.agents_per_team = 5;
    bcfg.obstacle_count        = 0;

    de::SimState sim;
    sim.bootstrap_battlefield(bcfg);
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.nav_queries_this_tick == 10,
          "nav_telemetry: 10 nav queries (5 agents x 2 teams)");
    check(snap.nav_blocked_cells == 0,
          "nav_telemetry: 0 blocked cells (no obstacles)");
}

// =================================================================
//  Main
// =================================================================

int main() {
    test_grid_empty_field();
    test_grid_blocked_cell();
    test_grid_flow_direction();
    test_grid_wall_with_gap();
    test_nav_regression_no_obstacle();
    test_nav_wall_agents_avoid();
    test_nav_wall_agents_cross_gap();
    test_nav_out_of_grid_no_bypass();
    test_nav_wall_no_crossing_through_wall();
    test_nav_invalid_config_fallback();
    test_nav_engage_overrides_flow();
    test_nav_telemetry();

    std::printf("\n--- NavTest: %d passed, %d failed ---\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
