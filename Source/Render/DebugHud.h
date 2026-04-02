#pragma once

#include <cstdint>

namespace de {

// HUD display mode.
enum class HudMode : uint8_t {
    Hidden  = 0,   // no overlay
    Compact = 1,   // condensed single-panel
    Full    = 2    // multi-panel with sections
};

// Toggle between Hidden and Full.
inline HudMode toggle_hud_visibility(HudMode m) {
    return (m == HudMode::Hidden) ? HudMode::Full : HudMode::Hidden;
}

// Cycle visible modes: Compact <-> Full. No-op if Hidden.
inline HudMode next_hud_mode(HudMode m) {
    if (m == HudMode::Hidden) return m;
    return (m == HudMode::Full) ? HudMode::Compact : HudMode::Full;
}

// One section of the debug HUD (titled group of lines).
struct DebugHudSection {
    static constexpr int k_max_lines = 6;
    static constexpr int k_line_len  = 48;
    static constexpr int k_title_len = 20;

    char title[k_title_len] = {};
    char lines[k_max_lines][k_line_len] = {};
    int  line_count = 0;

    void clear();
    void set_title(const char* t);
    void add(const char* text);
};

// Structured debug HUD: multiple titled sections with display mode.
struct DebugHudData {
    static constexpr int k_max_sections = 5;

    DebugHudSection sections[k_max_sections];
    int section_count = 0;
    HudMode mode = HudMode::Full;

    void clear();
    DebugHudSection& add_section(const char* title);
};

struct OverlayInstance;

// Extract structured HUD data from engine state.
// Fills sections based on mode:
//   Hidden  -> 0 sections
//   Compact -> 1 section (condensed overview)
//   Full    -> 4 sections (Runtime, Crowd, Budget, Controls)
//
// Contract:
//   cap_count       = agents not extracted (agent_count - extracted_count)
//                     NOT the same as RenderStats.dropped_count
//   frame_skipped   = true only on actual frame loss (e.g. resize failure)
//   renderer_active = false when running headless (no GPU)
//   cpu_time_ms     = pre-render CPU sum (begin + sim + presentation)
void extract_debug_hud(
    HudMode     mode,
    const char* scene,
    bool        paused,
    uint64_t    tick,
    bool        auto_frame,
    uint32_t    agent_count,
    uint32_t    extracted_count,
    uint32_t    visible_count,
    uint32_t    culled_count,
    uint32_t    cap_count,
    bool        frame_skipped,
    bool        renderer_active,
    bool        within_budget,
    double      cpu_time_ms,
    double      sim_time_ms,
    DebugHudData& out);

// Generate overlay pixel-quad instances from structured HUD data.
// Each section renders as a titled panel with its own background.
// Returns the number of instances written into out[].
uint32_t generate_hud_instances(
    const DebugHudData& data,
    float screen_w, float screen_h,
    OverlayInstance* out, uint32_t max_count);

}  // namespace de
