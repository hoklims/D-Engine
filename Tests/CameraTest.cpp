#include "Render/RenderCamera.h"
#include "Render/RenderFrame.h"

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

static bool near(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

// Reproduce HLSL mul(M, v) with column-major storage.
// m[0..3]=col0, m[4..7]=col1, m[8..11]=col2, m[12..15]=col3.
// result.x = m[0]*v.x + m[4]*v.y + m[8]*v.z  + m[12]*v.w
// result.y = m[1]*v.x + m[5]*v.y + m[9]*v.z  + m[13]*v.w
// result.z = m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w
// result.w = m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
struct Float4 { float x, y, z, w; };
static Float4 hlsl_mul(const float m[16], float vx, float vy, float vz, float vw) {
    return {
        m[0]*vx + m[4]*vy + m[8]*vz  + m[12]*vw,
        m[1]*vx + m[5]*vy + m[9]*vz  + m[13]*vw,
        m[2]*vx + m[6]*vy + m[10]*vz + m[14]*vw,
        m[3]*vx + m[7]*vy + m[11]*vz + m[15]*vw,
    };
}

// =================================================================
//  Empty frame -> stable fallback camera
// =================================================================

static void test_empty_frame() {
    de::RenderFrame frame = {};
    de::RenderCamera cam = de::auto_frame_crowd(frame, 16.0f / 9.0f);

    check(cam.center_x == 0.0f,          "empty: center_x == 0");
    check(cam.center_y == 0.0f,          "empty: center_y == 0");
    check(cam.half_width >= de::k_min_half_width, "empty: half_width >= min");
    check(cam.aspect > 0.0f,             "empty: aspect > 0");

    // Matrix must not contain NaN.
    float m[16];
    cam.build_ortho(m);
    bool has_nan = false;
    for (int i = 0; i < 16; ++i) {
        if (std::isnan(m[i]) || std::isinf(m[i])) has_nan = true;
    }
    check(!has_nan, "empty: ortho matrix has no NaN/Inf");
}

// =================================================================
//  Single agent -> camera centered on it, min half_width
// =================================================================

static void test_single_agent() {
    de::RenderFrame frame = {};
    frame.extracted_count = 1;
    frame.agents[0].x = 10.0f;
    frame.agents[0].y = -5.0f;

    de::RenderCamera cam = de::auto_frame_crowd(frame, 1.0f);

    check(near(cam.center_x, 10.0f),  "single: center_x == 10");
    check(near(cam.center_y, -5.0f),  "single: center_y == -5");
    check(cam.half_width >= de::k_min_half_width, "single: half_width >= min");
}

// =================================================================
//  Two agents -> camera frames both
// =================================================================

static void test_two_agents() {
    de::RenderFrame frame = {};
    frame.extracted_count = 2;
    frame.agents[0].x = -20.0f;
    frame.agents[0].y = 0.0f;
    frame.agents[1].x = 20.0f;
    frame.agents[1].y = 0.0f;

    de::RenderCamera cam = de::auto_frame_crowd(frame, 1.0f);

    check(near(cam.center_x, 0.0f), "two: center_x == 0");
    check(near(cam.center_y, 0.0f), "two: center_y == 0");
    // half_width must cover 20 * (1 + margin)
    float expected_hw = 20.0f * (1.0f + de::k_auto_frame_margin);
    check(cam.half_width >= expected_hw - 0.01f, "two: half_width covers extent");
}

// =================================================================
//  Translated scene -> camera follows
// =================================================================

static void test_translated_scene() {
    de::RenderFrame frame = {};
    frame.extracted_count = 4;
    // All agents shifted to (100, 200) area.
    frame.agents[0] = { 95.0f,  195.0f, 0, 0, 1.0f };
    frame.agents[1] = { 105.0f, 195.0f, 0, 0, 1.0f };
    frame.agents[2] = { 95.0f,  205.0f, 0, 0, 1.0f };
    frame.agents[3] = { 105.0f, 205.0f, 0, 0, 1.0f };

    de::RenderCamera cam = de::auto_frame_crowd(frame, 1.0f);

    check(near(cam.center_x, 100.0f), "translated: center_x == 100");
    check(near(cam.center_y, 200.0f), "translated: center_y == 200");
}

// =================================================================
//  Aspect ratio change -> projection coherent
// =================================================================

static void test_aspect_ratio() {
    de::RenderFrame frame = {};
    frame.extracted_count = 2;
    frame.agents[0].x = -10.0f;
    frame.agents[0].y = -10.0f;
    frame.agents[1].x =  10.0f;
    frame.agents[1].y =  10.0f;

    de::RenderCamera cam_wide = de::auto_frame_crowd(frame, 2.0f);
    de::RenderCamera cam_tall = de::auto_frame_crowd(frame, 0.5f);

    check(near(cam_wide.aspect, 2.0f),  "aspect: wide aspect == 2");
    check(near(cam_tall.aspect, 0.5f),  "aspect: tall aspect == 0.5");

    // Wide: half_height = hw / 2 must cover 10*(1+margin)
    float needed = 10.0f * (1.0f + de::k_auto_frame_margin);
    check(cam_wide.half_height() >= needed - 0.01f,
          "aspect: wide half_height covers y extent");
    check(cam_tall.half_width >= needed - 0.01f,
          "aspect: tall half_width covers x extent");
}

// =================================================================
//  Ortho matrix: point at center maps to NDC origin (HLSL mul)
// =================================================================

static void test_ortho_center_maps_to_origin() {
    de::RenderCamera cam;
    cam.center_x   = 30.0f;
    cam.center_y   = -20.0f;
    cam.half_width = 50.0f;
    cam.aspect     = 1.0f;

    float m[16];
    cam.build_ortho(m);

    // mul(ortho, float4(30, -20, 0, 1)) must give NDC (0, 0, *, 1).
    Float4 ndc = hlsl_mul(m, 30.0f, -20.0f, 0.0f, 1.0f);

    check(near(ndc.x, 0.0f), "ortho: center_x maps to NDC 0");
    check(near(ndc.y, 0.0f), "ortho: center_y maps to NDC 0");
    check(near(ndc.w, 1.0f), "ortho: w == 1");
}

// =================================================================
//  Ortho matrix: edge maps to NDC +/-1 (HLSL mul)
// =================================================================

static void test_ortho_edge_maps_to_ndc() {
    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 40.0f;
    cam.aspect     = 2.0f;  // half_height = 20

    float m[16];
    cam.build_ortho(m);

    // Right edge: (40, 0, 0, 1) -> NDC x = 1
    Float4 r = hlsl_mul(m, 40.0f, 0.0f, 0.0f, 1.0f);
    check(near(r.x, 1.0f), "ortho: right edge -> NDC x=1");

    // Top edge: (0, 20, 0, 1) -> NDC y = 1
    Float4 t = hlsl_mul(m, 0.0f, 20.0f, 0.0f, 1.0f);
    check(near(t.y, 1.0f), "ortho: top edge -> NDC y=1");

    // Left edge: (-40, 0, 0, 1) -> NDC x = -1
    Float4 l = hlsl_mul(m, -40.0f, 0.0f, 0.0f, 1.0f);
    check(near(l.x, -1.0f), "ortho: left edge -> NDC x=-1");

    // Bottom edge: (0, -20, 0, 1) -> NDC y = -1
    Float4 b = hlsl_mul(m, 0.0f, -20.0f, 0.0f, 1.0f);
    check(near(b.y, -1.0f), "ortho: bottom edge -> NDC y=-1");
}

// =================================================================
//  Half_height derivation
// =================================================================

static void test_half_height() {
    de::RenderCamera cam;
    cam.half_width = 100.0f;
    cam.aspect = 2.0f;

    check(near(cam.half_height(), 50.0f), "half_height: 100/2 == 50");

    cam.aspect = 0.5f;
    check(near(cam.half_height(), 200.0f), "half_height: 100/0.5 == 200");
}

// =================================================================
//  Y-dominated scene needs wider half_width
// =================================================================

static void test_y_dominated_scene() {
    de::RenderFrame frame = {};
    frame.extracted_count = 2;
    frame.agents[0].x = 0.0f;
    frame.agents[0].y = -50.0f;
    frame.agents[1].x = 0.0f;
    frame.agents[1].y = 50.0f;

    // Wide aspect: half_height = hw / 4, so hw must be large to cover y=50.
    de::RenderCamera cam = de::auto_frame_crowd(frame, 4.0f);

    float needed_hh = 50.0f * (1.0f + de::k_auto_frame_margin);
    check(cam.half_height() >= needed_hh - 0.01f,
          "y-dom: half_height covers y extent on wide aspect");
}

// =================================================================
//  Translated scene: ortho matrix centers correctly (HLSL mul)
// =================================================================

static void test_translated_ortho() {
    de::RenderCamera cam;
    cam.center_x   = 100.0f;
    cam.center_y   = -50.0f;
    cam.half_width = 20.0f;
    cam.aspect     = 1.0f;  // half_height = 20

    float m[16];
    cam.build_ortho(m);

    // Center -> NDC (0, 0)
    Float4 c = hlsl_mul(m, 100.0f, -50.0f, 0.0f, 1.0f);
    check(near(c.x, 0.0f), "trans-ortho: center -> NDC x=0");
    check(near(c.y, 0.0f), "trans-ortho: center -> NDC y=0");

    // Right edge: (120, -50) -> NDC x=1
    Float4 r = hlsl_mul(m, 120.0f, -50.0f, 0.0f, 1.0f);
    check(near(r.x, 1.0f), "trans-ortho: right edge -> NDC x=1");

    // Top edge: (100, -30) -> NDC y=1
    Float4 t = hlsl_mul(m, 100.0f, -30.0f, 0.0f, 1.0f);
    check(near(t.y, 1.0f), "trans-ortho: top edge -> NDC y=1");

    // Origin (0,0) should NOT be at NDC (0,0)
    Float4 o = hlsl_mul(m, 0.0f, 0.0f, 0.0f, 1.0f);
    check(!near(o.x, 0.0f), "trans-ortho: origin != NDC 0 (x)");
    check(!near(o.y, 0.0f), "trans-ortho: origin != NDC 0 (y)");
}

// =================================================================
//  Column-major layout: verify specific indices
// =================================================================

static void test_column_major_layout() {
    de::RenderCamera cam;
    cam.center_x   = 10.0f;
    cam.center_y   = 20.0f;
    cam.half_width = 40.0f;
    cam.aspect     = 2.0f;  // half_height = 20

    float m[16];
    cam.build_ortho(m);

    // Diagonal: scale terms at [0], [5], [10], [15].
    check(near(m[0],  1.0f / 40.0f), "layout: m[0] = 1/hw");
    check(near(m[5],  1.0f / 20.0f), "layout: m[5] = 1/hh");
    check(near(m[10], 1.0f),         "layout: m[10] = 1");
    check(near(m[15], 1.0f),         "layout: m[15] = 1");

    // Translation in col3: indices [12] and [13].
    check(near(m[12], -10.0f / 40.0f), "layout: m[12] = -cx/hw");
    check(near(m[13], -20.0f / 20.0f), "layout: m[13] = -cy/hh");

    // All other slots must be zero.
    int zero_indices[] = {1,2,3,4,6,7,8,9,11,14};
    bool all_zero = true;
    for (int idx : zero_indices) {
        if (m[idx] != 0.0f) all_zero = false;
    }
    check(all_zero, "layout: all non-diagonal/translation slots are 0");
}

// =================================================================

int main() {
    test_empty_frame();
    test_single_agent();
    test_two_agents();
    test_translated_scene();
    test_aspect_ratio();
    test_ortho_center_maps_to_origin();
    test_ortho_edge_maps_to_ndc();
    test_half_height();
    test_y_dominated_scene();
    test_translated_ortho();
    test_column_major_layout();

    std::printf("\nCameraTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
