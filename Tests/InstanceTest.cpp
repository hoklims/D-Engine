#include "Render/RenderFrame.h"
#include "Render/RenderStats.h"
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

// Mirror the Renderer stats contract on CPU side.
// agent_count is the true world count (before extraction cap).
// instance_count = min(extracted_count, instance_cap).
// dropped_count  = agent_count - instance_count.
static de::RenderStats compute_stats(const de::RenderFrame& frame,
                                     uint32_t instance_cap) {
    de::RenderStats s;
    s.agent_count     = frame.agent_count;
    s.extracted_count = frame.extracted_count;
    s.instance_count  = (frame.extracted_count < instance_cap)
                        ? frame.extracted_count : instance_cap;
    s.draw_call_count = (s.instance_count > 0) ? 1u : 0u;
    s.dropped_count   = frame.agent_count - s.instance_count;
    s.frame_skipped   = false;
    return s;
}

// =================================================================
//  Basic: 20 agents -> 20 instances, 1 draw call, 0 dropped
// =================================================================

static void test_basic_instance_count() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    de::RenderStats st = compute_stats(frame, de::k_max_render_agents);

    check(st.agent_count == 20,       "basic: agent_count == 20");
    check(st.extracted_count == 20,   "basic: extracted_count == 20");
    check(st.instance_count == 20,    "basic: instance_count == 20");
    check(st.draw_call_count == 1,    "basic: 1 draw call");
    check(st.dropped_count == 0,      "basic: 0 dropped");
}

// =================================================================
//  Empty scene: 0 instances, 0 draw calls
// =================================================================

static void test_empty_instance() {
    de::SimState sim;
    sim.bootstrap();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    de::RenderStats st = compute_stats(frame, de::k_max_render_agents);

    check(st.agent_count == 0,        "empty: agent_count == 0");
    check(st.extracted_count == 0,    "empty: extracted_count == 0");
    check(st.instance_count == 0,     "empty: instance_count == 0");
    check(st.draw_call_count == 0,    "empty: 0 draw calls");
    check(st.dropped_count == 0,      "empty: 0 dropped");
}

// =================================================================
//  World has more agents than extraction cap -> dropped_count > 0
// =================================================================

static void test_world_exceeds_extraction_cap() {
    // Synthetic frame: world has 5000 agents but extracted only 4096.
    de::RenderFrame frame = {};
    frame.agent_count     = 5000;
    frame.extracted_count = de::k_max_render_agents;  // 4096
    for (uint32_t i = 0; i < frame.extracted_count; ++i) {
        frame.agents[i].x = static_cast<float>(i);
        frame.agents[i].health_pct = 1.0f;
    }

    de::RenderStats st = compute_stats(frame, de::k_max_render_agents);

    check(st.agent_count == 5000,              "world>cap: agent_count == 5000");
    check(st.extracted_count == 4096,          "world>cap: extracted == 4096");
    check(st.instance_count == 4096,           "world>cap: instance == 4096");
    check(st.dropped_count == 904,             "world>cap: dropped == 904");
    check(st.instance_count + st.dropped_count == st.agent_count,
          "world>cap: invariant holds");
}

// =================================================================
//  Instance cap smaller than extracted -> double cap
// =================================================================

static void test_double_cap() {
    de::RenderFrame frame = {};
    frame.agent_count     = 200;
    frame.extracted_count = 100;  // extraction capped
    for (uint32_t i = 0; i < 100; ++i) {
        frame.agents[i].health_pct = 1.0f;
    }

    uint32_t small_instance_cap = 50;
    de::RenderStats st = compute_stats(frame, small_instance_cap);

    check(st.agent_count == 200,      "double-cap: agent_count == 200");
    check(st.extracted_count == 100,  "double-cap: extracted == 100");
    check(st.instance_count == 50,    "double-cap: instance == 50");
    check(st.dropped_count == 150,    "double-cap: dropped == 150");
    check(st.instance_count + st.dropped_count == st.agent_count,
          "double-cap: invariant holds");
}

// =================================================================
//  Stats invariant: instance + dropped == agent_count (various sizes)
// =================================================================

static void test_invariant_various() {
    uint32_t cases[][3] = {
        // { agent_count, extracted_count, instance_cap }
        { 0,    0,    4096 },
        { 10,   10,   4096 },
        { 4096, 4096, 4096 },
        { 5000, 4096, 4096 },
        { 100,  100,  50   },
        { 1,    1,    1    },
    };

    for (auto& c : cases) {
        de::RenderFrame frame = {};
        frame.agent_count     = c[0];
        frame.extracted_count = c[1];

        de::RenderStats st = compute_stats(frame, c[2]);

        bool ok = (st.instance_count + st.dropped_count == st.agent_count);
        check(ok, "invariant: inst + drop == agent_count");
    }
}

// =================================================================
//  Instance data per-agent: team colors and HP modulation
// =================================================================

static void test_instance_data_fields() {
    de::RenderFrame frame = {};
    frame.extracted_count = 2;
    frame.agent_count     = 2;
    frame.agents[0] = { 10.0f, 20.0f, 1, 0, 0.5f };
    frame.agents[1] = { -5.0f, 3.0f,  0, 0, 1.0f };

    check(frame.agents[0].x == 10.0f, "fields: agent[0].x == 10");
    check(frame.agents[0].y == 20.0f, "fields: agent[0].y == 20");
    check(frame.agents[1].x == -5.0f, "fields: agent[1].x == -5");
    check(frame.agents[0].health_pct == 0.5f, "fields: agent[0].hp == 0.5");
    check(frame.agents[0].team_id == 1, "fields: agent[0].team == 1");
}

// =================================================================
//  RenderFrame extraction still works (regression check)
// =================================================================

static void test_extraction_regression() {
    de::SimState sim;
    de::CrowdConfig cfg;
    cfg.agents_per_team = 15;
    sim.bootstrap_crowd(cfg);

    de::RenderFrame frame;
    uint32_t count = de::extract_render_frame(sim.world, 5, 10, frame);

    check(count == 30,                "regression: 30 agents extracted");
    check(frame.agent_count == 30,    "regression: agent_count == 30");
    check(frame.sim_tick == 5,        "regression: sim_tick == 5");
    check(frame.frame_id == 10,       "regression: frame_id == 10");
}

// =================================================================

int main() {
    test_basic_instance_count();
    test_empty_instance();
    test_world_exceeds_extraction_cap();
    test_double_cap();
    test_invariant_various();
    test_instance_data_fields();
    test_extraction_regression();

    std::printf("\nInstanceTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
