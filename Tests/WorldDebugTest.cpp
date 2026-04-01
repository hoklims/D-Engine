#include "Render/WorldDebugPass.h"

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

// =================================================================
//  Default config produces non-empty output
// =================================================================

static void test_default_config() {
    de::WorldDebugConfig cfg;
    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    check(data.count > 0, "default: count > 0");
    // Ground (1) + vertical lines + horizontal lines + 2 axes.
    // extent=100, spacing=10 -> n=10, 21 vert + 21 horiz + 1 ground + 2 axes = 45.
    check(data.count == 45, "default: count == 45");
}

// =================================================================
//  Ground only (grid disabled)
// =================================================================

static void test_ground_only() {
    de::WorldDebugConfig cfg;
    cfg.show_grid = false;

    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    check(data.count == 1, "ground-only: count == 1");

    const auto& g = data.items[0];
    check(g.pos_x == 0.0f && g.pos_y == 0.0f,
          "ground-only: centered at origin");
    check(g.half_sx == cfg.world_extent && g.half_sy == cfg.world_extent,
          "ground-only: covers world_extent");
}

// =================================================================
//  All disabled -> 0 instances
// =================================================================

static void test_all_disabled() {
    de::WorldDebugConfig cfg;
    cfg.show_ground = false;
    cfg.show_grid   = false;

    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    check(data.count == 0, "disabled: count == 0");
}

// =================================================================
//  Grid line count matches expected formula
// =================================================================

static void test_grid_line_count() {
    de::WorldDebugConfig cfg;
    cfg.show_ground  = false;
    cfg.world_extent = 50.0f;
    cfg.grid_spacing = 10.0f;

    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    // n = int(50/10) = 5, so -5..5 = 11 lines per axis.
    // 11 vert + 11 horiz + 2 axes = 24.
    check(data.count == 24, "grid-count: 24 instances for 50/10");
}

// =================================================================
//  Custom spacing
// =================================================================

static void test_custom_spacing() {
    de::WorldDebugConfig cfg;
    cfg.show_ground  = false;
    cfg.world_extent = 20.0f;
    cfg.grid_spacing = 5.0f;

    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    // n = int(20/5) = 4, so -4..4 = 9 lines per axis.
    // 9 vert + 9 horiz + 2 axes = 20.
    check(data.count == 20, "custom-spacing: 20 instances for 20/5");
}

// =================================================================
//  Origin axes are present and positioned at origin
// =================================================================

static void test_origin_axes() {
    de::WorldDebugConfig cfg;
    cfg.show_ground  = false;
    cfg.world_extent = 30.0f;
    cfg.grid_spacing = 10.0f;

    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    // Last two instances are the axes.
    check(data.count >= 2, "axes: at least 2 instances");

    const auto& x_axis = data.items[data.count - 2];
    const auto& y_axis = data.items[data.count - 1];

    check(x_axis.pos_x == 0.0f && x_axis.pos_y == 0.0f,
          "axes: x-axis at origin");
    check(y_axis.pos_x == 0.0f && y_axis.pos_y == 0.0f,
          "axes: y-axis at origin");

    // X axis: wide in X, thin in Y.
    check(x_axis.half_sx > x_axis.half_sy, "axes: x-axis wider than tall");
    // Y axis: thin in X, wide in Y.
    check(y_axis.half_sy > y_axis.half_sx, "axes: y-axis taller than wide");

    // X axis should be reddish (r > g, r > b).
    check(x_axis.r > x_axis.g && x_axis.r > x_axis.b,
          "axes: x-axis is reddish");
    // Y axis should be greenish (g > r, g > b).
    check(y_axis.g > y_axis.r && y_axis.g > y_axis.b,
          "axes: y-axis is greenish");
}

// =================================================================
//  No overflow with small spacing (many lines)
// =================================================================

static void test_no_overflow() {
    de::WorldDebugConfig cfg;
    cfg.world_extent = 1000.0f;
    cfg.grid_spacing = 1.0f;  // 2001 lines per axis -> would exceed cap

    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    check(data.count <= de::k_max_world_debug_instances,
          "overflow: count <= cap");
    check(data.count > 0, "overflow: count > 0");
}

// =================================================================
//  Idempotent: calling twice produces same result
// =================================================================

static void test_idempotent() {
    de::WorldDebugConfig cfg;
    de::WorldDebugData a, b;

    de::generate_world_debug(cfg, a);
    de::generate_world_debug(cfg, b);

    check(a.count == b.count, "idempotent: same count");
    bool same = true;
    for (uint32_t i = 0; i < a.count; ++i) {
        if (a.items[i].pos_x != b.items[i].pos_x ||
            a.items[i].pos_y != b.items[i].pos_y) {
            same = false;
            break;
        }
    }
    check(same, "idempotent: same data");
}

// =================================================================
//  Zero spacing -> no grid lines (avoids infinite loop)
// =================================================================

static void test_zero_spacing() {
    de::WorldDebugConfig cfg;
    cfg.grid_spacing = 0.0f;

    de::WorldDebugData data;
    de::generate_world_debug(cfg, data);

    // Only ground plane (grid skipped due to spacing <= 0).
    check(data.count == 1, "zero-spacing: only ground");
}

// =================================================================

int main() {
    test_default_config();
    test_ground_only();
    test_all_disabled();
    test_grid_line_count();
    test_custom_spacing();
    test_origin_axes();
    test_no_overflow();
    test_idempotent();
    test_zero_spacing();

    std::printf("\nWorldDebugTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
