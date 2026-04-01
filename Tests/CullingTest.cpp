#include "Render/ViewCulling.h"
#include "Render/RenderCamera.h"
#include "Render/RenderFrame.h"
#include "Render/RenderStats.h"

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

static bool near(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

// =================================================================
//  View bounds: correct AABB from camera
// =================================================================

static void test_view_bounds() {
    de::RenderCamera cam;
    cam.center_x   = 10.0f;
    cam.center_y   = 20.0f;
    cam.half_width = 50.0f;
    cam.aspect     = 2.0f;  // half_height = 25

    de::ViewBounds b = de::compute_view_bounds(cam, 0.0f);

    check(near(b.min_x, -40.0f), "bounds: min_x = cx - hw");
    check(near(b.max_x,  60.0f), "bounds: max_x = cx + hw");
    check(near(b.min_y,  -5.0f), "bounds: min_y = cy - hh");
    check(near(b.max_y,  45.0f), "bounds: max_y = cy + hh");
}

// =================================================================
//  View bounds: margin expands correctly
// =================================================================

static void test_view_bounds_margin() {
    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 10.0f;
    cam.aspect     = 1.0f;  // half_height = 10

    de::ViewBounds b = de::compute_view_bounds(cam, 2.0f);

    check(near(b.min_x, -12.0f), "margin: min_x = -10 - 2");
    check(near(b.max_x,  12.0f), "margin: max_x = 10 + 2");
    check(near(b.min_y, -12.0f), "margin: min_y = -10 - 2");
    check(near(b.max_y,  12.0f), "margin: max_y = 10 + 2");
}

// =================================================================
//  is_in_view: point inside/outside
// =================================================================

static void test_is_in_view() {
    de::ViewBounds b = { -10.0f, 10.0f, -5.0f, 5.0f };

    check(de::is_in_view(0.0f, 0.0f, b),     "in_view: origin inside");
    check(de::is_in_view(10.0f, 5.0f, b),    "in_view: corner inside");
    check(de::is_in_view(-10.0f, -5.0f, b),  "in_view: opposite corner inside");
    check(!de::is_in_view(11.0f, 0.0f, b),   "in_view: right outside");
    check(!de::is_in_view(-11.0f, 0.0f, b),  "in_view: left outside");
    check(!de::is_in_view(0.0f, 6.0f, b),    "in_view: top outside");
    check(!de::is_in_view(0.0f, -6.0f, b),   "in_view: bottom outside");
}

// =================================================================
//  All agents inside view -> all visible
// =================================================================

static void test_all_visible() {
    de::RenderFrame frame = {};
    frame.agent_count     = 4;
    frame.extracted_count = 4;
    frame.agents[0] = { 0.0f,  0.0f, 0, 0, 1.0f };
    frame.agents[1] = { 5.0f,  3.0f, 0, 0, 1.0f };
    frame.agents[2] = {-5.0f, -3.0f, 0, 0, 1.0f };
    frame.agents[3] = { 2.0f, -1.0f, 0, 0, 1.0f };

    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 50.0f;
    cam.aspect     = 16.0f / 9.0f;

    uint32_t visible = de::cull_render_frame(frame, cam);

    check(visible == 4,               "all_vis: 4 visible");
    check(frame.agent_count == 4,     "all_vis: agent_count unchanged");
    check(frame.extracted_count == 4, "all_vis: extracted_count unchanged");
}

// =================================================================
//  All agents outside view -> all culled
// =================================================================

static void test_all_culled() {
    de::RenderFrame frame = {};
    frame.agent_count     = 3;
    frame.extracted_count = 3;
    frame.agents[0] = { 200.0f,  200.0f, 0, 0, 1.0f };
    frame.agents[1] = {-200.0f,  200.0f, 0, 0, 1.0f };
    frame.agents[2] = { 200.0f, -200.0f, 0, 0, 1.0f };

    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 10.0f;
    cam.aspect     = 1.0f;

    uint32_t visible = de::cull_render_frame(frame, cam);

    check(visible == 0,               "all_cull: 0 visible");
    check(frame.agent_count == 3,     "all_cull: agent_count unchanged");
    check(frame.extracted_count == 3, "all_cull: extracted_count unchanged");
}

// =================================================================
//  Partial cull: some in, some out
// =================================================================

static void test_partial_cull() {
    de::RenderFrame frame = {};
    frame.agent_count     = 5;
    frame.extracted_count = 5;
    // Inside view (cam at origin, hw=10, aspect=1 -> hh=10, margin=0.5)
    frame.agents[0] = { 0.0f, 0.0f, 0, 0, 1.0f };  // in
    frame.agents[1] = { 5.0f, 5.0f, 1, 0, 0.8f };   // in
    // Outside view
    frame.agents[2] = { 50.0f, 0.0f, 0, 0, 1.0f };  // out
    frame.agents[3] = { 0.0f, 50.0f, 0, 0, 1.0f };  // out
    // Inside
    frame.agents[4] = {-3.0f, -3.0f, 1, 0, 0.5f };  // in

    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 10.0f;
    cam.aspect     = 1.0f;

    uint32_t visible = de::cull_render_frame(frame, cam);

    check(visible == 3,               "partial: 3 visible");
    check(frame.agent_count == 5,     "partial: agent_count unchanged");
    check(frame.extracted_count == 5, "partial: extracted_count unchanged");

    // Verify compacted data: the 3 visible agents are at front.
    check(near(frame.agents[0].x, 0.0f), "partial: agent[0] survived");
    check(near(frame.agents[1].x, 5.0f), "partial: agent[1] survived");
    check(near(frame.agents[2].x,-3.0f), "partial: agent[2] = moved from [4]");
}

// =================================================================
//  Empty frame -> 0 visible
// =================================================================

static void test_empty_frame() {
    de::RenderFrame frame = {};

    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 50.0f;
    cam.aspect     = 1.0f;

    uint32_t visible = de::cull_render_frame(frame, cam);

    check(visible == 0, "empty: 0 visible");
}

// =================================================================
//  Margin saves edge agent: just outside camera but inside margin
// =================================================================

static void test_margin_saves_edge() {
    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 10.0f;
    cam.aspect     = 1.0f;  // hh = 10

    // Agent at x=10.3 -- outside camera hw (10) but inside margin (0.5).
    de::RenderFrame frame = {};
    frame.agent_count     = 1;
    frame.extracted_count = 1;
    frame.agents[0] = { 10.3f, 0.0f, 0, 0, 1.0f };

    uint32_t visible = de::cull_render_frame(frame, cam);
    check(visible == 1, "margin: agent at 10.3 visible (margin=0.5)");

    // Agent at x=10.6 -- outside camera hw AND outside margin.
    frame.agents[0] = { 10.6f, 0.0f, 0, 0, 1.0f };
    visible = de::cull_render_frame(frame, cam);
    check(visible == 0, "margin: agent at 10.6 culled");
}

// =================================================================
//  Translated camera: agents visible relative to camera center
// =================================================================

static void test_translated_camera() {
    de::RenderCamera cam;
    cam.center_x   = 100.0f;
    cam.center_y   = 100.0f;
    cam.half_width = 5.0f;
    cam.aspect     = 1.0f;

    de::RenderFrame frame = {};
    frame.agent_count     = 3;
    frame.extracted_count = 3;
    frame.agents[0] = { 100.0f, 100.0f, 0, 0, 1.0f };  // center -> in
    frame.agents[1] = {   0.0f,   0.0f, 0, 0, 1.0f };  // far -> out
    frame.agents[2] = { 103.0f,  98.0f, 0, 0, 1.0f };  // near center -> in

    uint32_t visible = de::cull_render_frame(frame, cam);

    check(visible == 2, "translated: 2 visible near cam center");
}

// =================================================================
//  Determinism: same input -> same output
// =================================================================

static void test_determinism() {
    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 10.0f;
    cam.aspect     = 1.0f;

    auto make_frame = []() {
        de::RenderFrame f = {};
        f.agent_count     = 6;
        f.extracted_count = 6;
        f.agents[0] = { 0.0f,  0.0f, 0, 0, 1.0f };   // in
        f.agents[1] = { 50.0f, 0.0f, 0, 0, 1.0f };   // out
        f.agents[2] = { 5.0f,  5.0f, 1, 0, 0.5f };   // in
        f.agents[3] = {-50.0f, 0.0f, 0, 0, 1.0f };   // out
        f.agents[4] = {-3.0f, -3.0f, 1, 0, 0.8f };   // in
        f.agents[5] = { 0.0f, 50.0f, 0, 0, 1.0f };   // out
        return f;
    };

    de::RenderFrame f1 = make_frame();
    de::RenderFrame f2 = make_frame();

    uint32_t v1 = de::cull_render_frame(f1, cam);
    uint32_t v2 = de::cull_render_frame(f2, cam);

    check(v1 == v2, "determinism: same visible count");
    check(v1 == 3,  "determinism: 3 visible");

    bool same = true;
    for (uint32_t i = 0; i < v1; ++i) {
        if (!near(f1.agents[i].x, f2.agents[i].x) ||
            !near(f1.agents[i].y, f2.agents[i].y)) {
            same = false;
        }
    }
    check(same, "determinism: compacted arrays identical");
}

// =================================================================
//  Stats invariants: visible + culled == extracted
// =================================================================

static void test_stats_invariants() {
    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 10.0f;
    cam.aspect     = 1.0f;

    de::RenderFrame frame = {};
    frame.agent_count     = 8;
    frame.extracted_count = 8;
    // 4 inside, 4 outside.
    frame.agents[0] = { 0.0f,  0.0f, 0, 0, 1.0f };
    frame.agents[1] = { 5.0f,  5.0f, 0, 0, 1.0f };
    frame.agents[2] = {-5.0f, -5.0f, 0, 0, 1.0f };
    frame.agents[3] = { 3.0f, -3.0f, 0, 0, 1.0f };
    frame.agents[4] = { 50.0f, 0.0f, 0, 0, 1.0f };
    frame.agents[5] = { 0.0f, 50.0f, 0, 0, 1.0f };
    frame.agents[6] = {-50.0f, 0.0f, 0, 0, 1.0f };
    frame.agents[7] = { 0.0f,-50.0f, 0, 0, 1.0f };

    uint32_t extracted = frame.extracted_count;
    uint32_t visible   = de::cull_render_frame(frame, cam);
    uint32_t culled    = extracted - visible;

    check(visible == 4,                     "invariant: 4 visible");
    check(culled == 4,                      "invariant: 4 culled");
    check(visible + culled == extracted,    "invariant: vis + cull == extracted");

    // Simulate stats as Engine would build them.
    de::RenderStats st;
    st.agent_count     = frame.agent_count;
    st.extracted_count = extracted;
    st.visible_count   = visible;
    st.culled_count    = culled;
    st.instance_count  = visible;  // all visible fit in instance buffer
    st.dropped_count   = st.agent_count - st.instance_count;

    check(st.visible_count + st.culled_count == st.extracted_count,
          "stats: vis + cull == extracted");
    check(st.instance_count + st.dropped_count == st.agent_count,
          "stats: inst + drop == agent_count");
    check(st.instance_count <= st.visible_count,
          "stats: inst <= visible");
}

// =================================================================
//  Data integrity: culled frame preserves agent fields
// =================================================================

static void test_data_integrity() {
    de::RenderFrame frame = {};
    frame.agent_count     = 3;
    frame.extracted_count = 3;
    frame.agents[0] = { 100.0f, 0.0f, 0, 0, 1.0f };   // out
    frame.agents[1] = {   0.0f, 0.0f, 1, 2, 0.7f, 0.5f, 0.5f, true }; // in
    frame.agents[2] = { 100.0f, 0.0f, 0, 0, 1.0f };   // out

    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 10.0f;
    cam.aspect     = 1.0f;

    uint32_t visible = de::cull_render_frame(frame, cam);

    check(visible == 1,                        "integrity: 1 visible");
    check(frame.agents[0].team_id == 1,        "integrity: team_id preserved");
    check(frame.agents[0].lod_tier == 2,       "integrity: lod_tier preserved");
    check(near(frame.agents[0].health_pct, 0.7f), "integrity: health_pct preserved");
    check(near(frame.agents[0].dir_x, 0.5f),  "integrity: dir_x preserved");
    check(near(frame.agents[0].dir_y, 0.5f),  "integrity: dir_y preserved");
    check(frame.agents[0].has_target == true,  "integrity: has_target preserved");
}

// =================================================================

int main() {
    test_view_bounds();
    test_view_bounds_margin();
    test_is_in_view();
    test_all_visible();
    test_all_culled();
    test_partial_cull();
    test_empty_frame();
    test_margin_saves_edge();
    test_translated_camera();
    test_determinism();
    test_stats_invariants();
    test_data_integrity();

    std::printf("\nCullingTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
