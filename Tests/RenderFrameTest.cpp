#include "Render/RenderFrame.h"
#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"

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
//  Basic extraction from a bootstrapped crowd scene
// =================================================================

static void test_extract_basic() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 10;
    sim.bootstrap_crowd(cfg);

    de::RenderFrame frame;
    uint32_t count = de::extract_render_frame(sim.world, 0, 1, frame);

    check(count == 20,                "basic: 20 agents extracted");
    check(frame.agent_count == 20,    "basic: agent_count == 20");
    check(frame.extracted_count == 20,"basic: extracted_count == 20");
    check(frame.sim_tick == 0,        "basic: sim_tick == 0");
    check(frame.frame_id == 1,        "basic: frame_id == 1");
}

// =================================================================
//  Team distribution is correct
// =================================================================

static void test_extract_teams() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    uint32_t t0 = 0, t1 = 0;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        if (frame.agents[i].team_id == 0) ++t0;
        if (frame.agents[i].team_id == 1) ++t1;
    }

    check(t0 == 10, "teams: 10 on team 0");
    check(t1 == 10, "teams: 10 on team 1");
}

// =================================================================
//  Positions are non-zero (agents spawn away from origin)
// =================================================================

static void test_extract_positions() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    bool any_nonzero = false;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        if (frame.agents[i].x != 0.0f || frame.agents[i].y != 0.0f) {
            any_nonzero = true;
            break;
        }
    }
    check(any_nonzero, "positions: at least one agent not at origin");
}

// =================================================================
//  Health is 1.0 at initial spawn
// =================================================================

static void test_extract_health() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    bool all_full = true;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        if (frame.agents[i].health_pct < 0.99f) {
            all_full = false;
            break;
        }
    }
    check(all_full, "health: all agents at full HP");
}

// =================================================================
//  Extraction is stable across ticks (no deaths when far apart)
// =================================================================

static void test_extract_stability() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing = 100.0f;
    sim.bootstrap_crowd(cfg);

    de::RenderFrame f1, f2, f3;

    de::extract_render_frame(sim.world, 0, 0, f1);
    sim.tick(1.0 / 60.0);
    de::extract_render_frame(sim.world, 1, 1, f2);
    sim.tick(1.0 / 60.0);
    de::extract_render_frame(sim.world, 2, 2, f3);

    check(f1.extracted_count == 10, "stability: 10 agents at tick 0");
    check(f2.extracted_count == 10, "stability: 10 agents at tick 1");
    check(f3.extracted_count == 10, "stability: 10 agents at tick 2");
}

// =================================================================
//  Extraction after deaths reduces count
// =================================================================

static void test_extract_after_deaths() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 1.0f;
    cfg.attack_range    = 5.0f;
    cfg.attack_damage   = 200.0f;
    cfg.attack_interval = 0.0f;
    sim.bootstrap_crowd(cfg);

    for (int i = 0; i < 10; ++i)
        sim.tick(1.0 / 60.0);

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 10, 10, frame);

    check(frame.extracted_count < 10,
          "after deaths: fewer agents extracted");
}

// =================================================================
//  Empty world produces zero-count frame
// =================================================================

static void test_extract_empty() {
    de::SimState sim;
    sim.bootstrap();

    de::RenderFrame frame;
    uint32_t count = de::extract_render_frame(sim.world, 0, 0, frame);

    check(count == 0, "empty: 0 crowd agents extracted");
    check(frame.agent_count == 0, "empty: agent_count == 0");
}

// =================================================================
//  Direction defaults to (0,1) at spawn (zero velocity)
// =================================================================

static void test_extract_direction_default() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    bool all_default = true;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        const auto& a = frame.agents[i];
        if (a.dir_x != 0.0f || a.dir_y != 1.0f) {
            all_default = false;
            break;
        }
    }
    check(all_default, "dir-default: (0,1) at spawn (v=0)");
}

// =================================================================
//  Direction tracks velocity after ticks
// =================================================================

static void test_extract_direction_after_ticks() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 10.0f;
    sim.bootstrap_crowd(cfg);

    for (int i = 0; i < 30; ++i)
        sim.tick(1.0 / 60.0);

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 30, 30, frame);

    // After 30 ticks with close teams, agents should be moving and targeting.
    bool any_targeted = false;
    bool any_nondefault_dir = false;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        const auto& a = frame.agents[i];
        if (a.has_target) any_targeted = true;
        if (a.dir_x != 0.0f || a.dir_y != 1.0f)
            any_nondefault_dir = true;
    }
    check(any_targeted, "dir-ticks: some agents have target");
    check(any_nondefault_dir, "dir-ticks: some agents facing non-default");
}

// =================================================================
//  Direction is normalized when non-zero
// =================================================================

static void test_extract_direction_normalized() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 5;
    cfg.team_spacing    = 10.0f;
    sim.bootstrap_crowd(cfg);

    for (int i = 0; i < 20; ++i)
        sim.tick(1.0 / 60.0);

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 20, 20, frame);

    bool all_unit = true;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        const auto& a = frame.agents[i];
        float len = a.dir_x * a.dir_x + a.dir_y * a.dir_y;
        if (len < 0.99f || len > 1.01f) {
            all_unit = false;
            break;
        }
    }
    check(all_unit, "dir-norm: all directions are unit length");
}

// =================================================================
//  Empty world: direction fields have safe defaults
// =================================================================

static void test_extract_direction_empty() {
    de::SimState sim;
    sim.bootstrap();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    check(frame.extracted_count == 0, "dir-empty: no agents");
    // Verify default-initialized items are safe.
    check(frame.agents[0].dir_x == 0.0f,    "dir-empty: default dir_x");
    check(frame.agents[0].dir_y == 1.0f,    "dir-empty: default dir_y");
    check(!frame.agents[0].has_target,       "dir-empty: default no target");
}

// =================================================================
//  DesiredDirection fallback: stationary agent with intent keeps facing
// =================================================================

static void test_direction_desired_fallback() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 200.0f;   // far apart = no velocity
    sim.bootstrap_crowd(cfg);

    // Manually set DesiredDirection on the first agent to a known value.
    // Velocity stays zero (teams too far apart to move yet at tick 0).
    uint32_t found = 0;
    sim.world.each<de::CrowdAgent, de::Velocity, de::DesiredDirection>(
        [&](de::EntityId, de::CrowdAgent&, de::Velocity& v, de::DesiredDirection& dd) {
            if (found == 0) {
                v.dx  = 0.0f;  v.dy  = 0.0f;   // stationary
                dd.dx = 1.0f;  dd.dy = 0.0f;    // intent: face right
            }
            ++found;
        });

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    // First extracted agent should face right via DesiredDirection fallback.
    bool ok = false;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        const auto& a = frame.agents[i];
        float dx = a.dir_x - 1.0f;
        float dy = a.dir_y - 0.0f;
        if (dx * dx + dy * dy < 0.01f) { ok = true; break; }
    }
    check(ok, "dd-fallback: stationary agent faces DesiredDirection");
}

// =================================================================
//  has_target reflects Target.has_target, not velocity
// =================================================================

static void test_has_target_contract() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 200.0f;
    sim.bootstrap_crowd(cfg);

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    // At spawn, no targets selected -> all has_target == false.
    bool none_targeted = true;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        if (frame.agents[i].has_target) { none_targeted = false; break; }
    }
    check(none_targeted, "target: none at spawn");

    // After ticks with close teams, targets should appear.
    de::SimState sim2;
    de::CrowdConfig cfg2;
    cfg2.agents_per_team = 5;
    cfg2.team_spacing    = 5.0f;
    sim2.bootstrap_crowd(cfg2);
    for (int i = 0; i < 5; ++i)
        sim2.tick(1.0 / 60.0);

    de::RenderFrame f2;
    de::extract_render_frame(sim2.world, 5, 5, f2);

    bool any_targeted = false;
    for (uint32_t i = 0; i < f2.extracted_count; ++i) {
        if (f2.agents[i].has_target) { any_targeted = true; break; }
    }
    check(any_targeted, "target: some targeted after ticks");
}

// =================================================================
//  has_target can be true while velocity is zero
// =================================================================

static void test_target_independent_of_velocity() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 1;
    cfg.team_spacing    = 200.0f;
    sim.bootstrap_crowd(cfg);

    // Manually set Target.has_target=true and Velocity=0 on first agent.
    uint32_t found = 0;
    sim.world.each<de::CrowdAgent, de::Velocity, de::Target>(
        [&](de::EntityId, de::CrowdAgent&, de::Velocity& v, de::Target& t) {
            if (found == 0) {
                v.dx = 0.0f;  v.dy = 0.0f;
                t.has_target = true;
            }
            ++found;
        });

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    bool ok = false;
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        if (frame.agents[i].has_target) { ok = true; break; }
    }
    check(ok, "target-vel: has_target true despite zero velocity");
}

// =================================================================

int main() {
    test_extract_basic();
    test_extract_teams();
    test_extract_positions();
    test_extract_health();
    test_extract_stability();
    test_extract_after_deaths();
    test_extract_empty();
    test_extract_direction_default();
    test_extract_direction_after_ticks();
    test_extract_direction_normalized();
    test_extract_direction_empty();
    test_direction_desired_fallback();
    test_has_target_contract();
    test_target_independent_of_velocity();

    std::printf("\nRenderFrameTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
