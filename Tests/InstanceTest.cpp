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

// Simulate what Renderer does on CPU side: compute instance count and stats
// from a RenderFrame, without GPU. This validates the instancing contract.
static de::RenderStats compute_stats(const de::RenderFrame& frame,
                                     uint32_t instance_cap) {
    de::RenderStats s;
    s.extracted_count = frame.extracted_count;
    s.instance_count  = (frame.extracted_count < instance_cap)
                        ? frame.extracted_count : instance_cap;
    s.draw_call_count = (s.instance_count > 0) ? 1u : 0u;
    s.dropped_count   = (frame.extracted_count > instance_cap)
                        ? (frame.extracted_count - instance_cap) : 0u;
    return s;
}

// =================================================================
//  Basic: 20 agents -> 20 instances, 1 draw call
// =================================================================

static void test_basic_instance_count() {
    de::SimState sim;
    sim.bootstrap_crowd();

    de::RenderFrame frame;
    de::extract_render_frame(sim.world, 0, 0, frame);

    de::RenderStats st = compute_stats(frame, de::k_max_render_agents);

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

    check(st.extracted_count == 0,    "empty: extracted_count == 0");
    check(st.instance_count == 0,     "empty: instance_count == 0");
    check(st.draw_call_count == 0,    "empty: 0 draw calls");
    check(st.dropped_count == 0,      "empty: 0 dropped");
}

// =================================================================
//  Cap hit: more agents than instance cap -> dropped
// =================================================================

static void test_cap_hit() {
    // Simulate a frame with more agents than a small cap.
    de::RenderFrame frame = {};
    frame.extracted_count = 100;
    for (uint32_t i = 0; i < 100; ++i) {
        frame.agents[i].x = static_cast<float>(i);
        frame.agents[i].y = 0.0f;
        frame.agents[i].team_id = 0;
        frame.agents[i].health_pct = 1.0f;
    }

    uint32_t small_cap = 50;
    de::RenderStats st = compute_stats(frame, small_cap);

    check(st.extracted_count == 100,  "cap: extracted_count == 100");
    check(st.instance_count == 50,    "cap: instance_count capped at 50");
    check(st.draw_call_count == 1,    "cap: 1 draw call");
    check(st.dropped_count == 50,     "cap: 50 dropped");
}

// =================================================================
//  Exactly at cap: no drop
// =================================================================

static void test_exactly_at_cap() {
    de::RenderFrame frame = {};
    frame.extracted_count = 64;
    for (uint32_t i = 0; i < 64; ++i) {
        frame.agents[i].x = static_cast<float>(i);
        frame.agents[i].health_pct = 1.0f;
    }

    de::RenderStats st = compute_stats(frame, 64);

    check(st.instance_count == 64,  "exact cap: instance_count == 64");
    check(st.dropped_count == 0,    "exact cap: 0 dropped");
}

// =================================================================
//  Stats coherence: instance + dropped == extracted
// =================================================================

static void test_stats_coherence() {
    de::RenderFrame frame = {};
    frame.extracted_count = 200;
    for (uint32_t i = 0; i < 200; ++i) {
        frame.agents[i].health_pct = 1.0f;
    }

    uint32_t cap = 128;
    de::RenderStats st = compute_stats(frame, cap);

    check(st.instance_count + st.dropped_count == st.extracted_count,
          "coherence: instance + dropped == extracted");
}

// =================================================================
//  Instance data per-agent: team colors and HP modulation
// =================================================================

static void test_instance_data_fields() {
    de::RenderFrame frame = {};
    frame.extracted_count = 2;
    frame.agents[0] = { 10.0f, 20.0f, 1, 0, 0.5f };
    frame.agents[1] = { -5.0f, 3.0f,  0, 0, 1.0f };

    // Verify positions are preserved in frame.
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
    test_cap_hit();
    test_exactly_at_cap();
    test_stats_coherence();
    test_instance_data_fields();
    test_extraction_regression();

    std::printf("\nInstanceTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
