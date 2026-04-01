#include "Render/WorldDebugPass.h"

namespace de {

void generate_world_debug(const WorldDebugConfig& config, WorldDebugData& out) {
    out.count = 0;

    auto push = [&](float px, float py, float hsx, float hsy,
                     float r, float g, float b, float a) {
        if (out.count >= k_max_world_debug_instances) return;
        out.items[out.count++] = { px, py, hsx, hsy, r, g, b, a };
    };

    float ext = config.world_extent;

    // Ground plane: slightly lighter than the clear color (0.08, 0.08, 0.12).
    if (config.show_ground) {
        push(0.0f, 0.0f, ext, ext, 0.11f, 0.12f, 0.15f, 1.0f);
    }

    if (!config.show_grid) return;

    float sp = config.grid_spacing;
    if (sp <= 0.0f) return;

    float line_w = 0.06f;  // half-thickness of regular grid lines

    // Grid color: subtle, slightly brighter than ground.
    float gr = 0.17f, gg = 0.18f, gb = 0.22f;

    // Number of lines from center to edge.
    int n = static_cast<int>(ext / sp);

    // Vertical grid lines.
    for (int i = -n; i <= n; ++i) {
        push(static_cast<float>(i) * sp, 0.0f, line_w, ext, gr, gg, gb, 1.0f);
    }

    // Horizontal grid lines.
    for (int i = -n; i <= n; ++i) {
        push(0.0f, static_cast<float>(i) * sp, ext, line_w, gr, gg, gb, 1.0f);
    }

    // Origin axes: thicker, colored.
    float axis_w = 0.12f;
    // X axis (red).
    push(0.0f, 0.0f, ext, axis_w, 0.40f, 0.12f, 0.12f, 1.0f);
    // Y axis (green).
    push(0.0f, 0.0f, axis_w, ext, 0.12f, 0.40f, 0.12f, 1.0f);
}

} // namespace de
