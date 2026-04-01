#include "Render/WorldDebugPass.h"

namespace de {

void generate_world_debug(const WorldDebugConfig& config, WorldDebugData& out) {
    out.count = 0;

    // Axes are the most useful spatial reference. Reserve their slots first
    // so that a dense grid can never push them out of the instance budget.
    static constexpr uint32_t k_axis_count = 2;

    // Grid lines may use at most (cap - ground - axes) slots.
    uint32_t ground_slots = config.show_ground ? 1u : 0u;
    uint32_t axis_slots   = config.show_grid   ? k_axis_count : 0u;
    uint32_t grid_budget  = k_max_world_debug_instances - ground_slots - axis_slots;

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

    // Emit grid lines up to the reserved budget.
    uint32_t grid_emitted = 0;

    // Vertical grid lines.
    for (int i = -n; i <= n && grid_emitted < grid_budget; ++i) {
        push(static_cast<float>(i) * sp, 0.0f, line_w, ext, gr, gg, gb, 1.0f);
        ++grid_emitted;
    }

    // Horizontal grid lines.
    for (int i = -n; i <= n && grid_emitted < grid_budget; ++i) {
        push(0.0f, static_cast<float>(i) * sp, ext, line_w, gr, gg, gb, 1.0f);
        ++grid_emitted;
    }

    // Origin axes: always emitted last, slots guaranteed by budget above.
    float axis_w = 0.12f;
    // X axis (red).
    push(0.0f, 0.0f, ext, axis_w, 0.40f, 0.12f, 0.12f, 1.0f);
    // Y axis (green).
    push(0.0f, 0.0f, axis_w, ext, 0.12f, 0.40f, 0.12f, 1.0f);
}

} // namespace de
