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
//  Ortho matrix: point at center maps to NDC origin
// =================================================================

static void test_ortho_center_maps_to_origin() {
    de::RenderCamera cam;
    cam.center_x   = 30.0f;
    cam.center_y   = -20.0f;
    cam.half_width = 50.0f;
    cam.aspect     = 1.0f;

    float m[16];
    cam.build_ortho(m);

    // Apply matrix to (30, -20, 0, 1). Row-major multiplication.
    float x = m[0] * 30.0f + m[1] * (-20.0f) + m[2] * 0.0f + m[3] * 1.0f;
    float y = m[4] * 30.0f + m[5] * (-20.0f) + m[6] * 0.0f + m[7] * 1.0f;

    check(near(x, 0.0f), "ortho: center_x maps to NDC 0");
    check(near(y, 0.0f), "ortho: center_y maps to NDC 0");
}

// =================================================================
//  Ortho matrix: edge maps to NDC +/-1
// =================================================================

static void test_ortho_edge_maps_to_ndc() {
    de::RenderCamera cam;
    cam.center_x   = 0.0f;
    cam.center_y   = 0.0f;
    cam.half_width = 40.0f;
    cam.aspect     = 2.0f;  // half_height = 20

    float m[16];
    cam.build_ortho(m);

    // Right edge: (40, 0) -> NDC x = 1
    float x_right = m[0] * 40.0f + m[3];
    check(near(x_right, 1.0f), "ortho: right edge -> NDC x=1");

    // Top edge: (0, 20) -> NDC y = 1
    float y_top = m[5] * 20.0f + m[7];
    check(near(y_top, 1.0f), "ortho: top edge -> NDC y=1");
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

    std::printf("\nCameraTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
