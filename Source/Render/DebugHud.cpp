#include "Render/DebugHud.h"
#include "Render/DebugOverlay.h"
#include "Render/BitmapFont.h"

#include <cstdio>
#include <cstring>

namespace de {

// -- DebugHudSection --------------------------------------------------------

void DebugHudSection::clear() {
    title[0]   = '\0';
    line_count = 0;
}

void DebugHudSection::set_title(const char* t) {
    int i = 0;
    while (i < k_title_len - 1 && t[i]) {
        title[i] = t[i];
        ++i;
    }
    title[i] = '\0';
}

void DebugHudSection::add(const char* text) {
    if (line_count >= k_max_lines) return;
    char* dst = lines[line_count];
    int i = 0;
    while (i < k_line_len - 1 && text[i]) {
        dst[i] = text[i];
        ++i;
    }
    dst[i] = '\0';
    ++line_count;
}

// -- DebugHudData -----------------------------------------------------------

void DebugHudData::clear() {
    section_count = 0;
    for (int i = 0; i < k_max_sections; ++i)
        sections[i].clear();
}

DebugHudSection& DebugHudData::add_section(const char* title) {
    if (section_count < k_max_sections) {
        auto& s = sections[section_count];
        s.clear();
        s.set_title(title);
        ++section_count;
        return s;
    }
    return sections[k_max_sections - 1];
}

// -- Extraction -------------------------------------------------------------

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
    uint32_t    dropped_count,
    bool        frame_skipped,
    bool        renderer_active,
    bool        within_budget,
    double      cpu_time_ms,
    double      sim_time_ms,
    DebugHudData& out)
{
    out.clear();
    out.mode = mode;

    if (mode == HudMode::Hidden) return;

    char buf[DebugHudSection::k_line_len];

    if (mode == HudMode::Compact) {
        auto& s = out.add_section("D-ENGINE");

        std::snprintf(buf, sizeof(buf), "%s | %s | t:%llu | cam:%s",
                      scene,
                      paused ? "PAUSED" : "RUN",
                      static_cast<unsigned long long>(tick),
                      auto_frame ? "auto" : "manual");
        s.add(buf);

        std::snprintf(buf, sizeof(buf), "A:%u vis:%u cul:%u | %s | %.1fms",
                      agent_count, visible_count, culled_count,
                      within_budget ? "OK" : "OVER",
                      cpu_time_ms);
        s.add(buf);

        if (frame_skipped) s.add("FRAME SKIPPED");
        else if (!renderer_active) s.add("Render: OFF");

        return;
    }

    // -- Full mode: 4 sections. --

    // Runtime.
    {
        auto& s = out.add_section("RUNTIME");
        std::snprintf(buf, sizeof(buf), "Scene: %s", scene);
        s.add(buf);
        s.add(paused ? "State: PAUSED" : "State: RUNNING");
        std::snprintf(buf, sizeof(buf), "Tick: %llu",
                      static_cast<unsigned long long>(tick));
        s.add(buf);
        s.add(auto_frame ? "Camera: auto" : "Camera: manual");
    }

    // Crowd.
    {
        auto& s = out.add_section("CROWD");
        std::snprintf(buf, sizeof(buf), "Agents: %u  ext:%u",
                      agent_count, extracted_count);
        s.add(buf);
        std::snprintf(buf, sizeof(buf), "Vis:%u  cull:%u  drop:%u",
                      visible_count, culled_count, dropped_count);
        s.add(buf);
    }

    // Budget.
    {
        auto& s = out.add_section("BUDGET");
        s.add(within_budget ? "Budget: OK" : "Budget: OVER");
        if (frame_skipped) s.add("Frame: SKIPPED");
        else if (!renderer_active) s.add("Render: OFF");
        std::snprintf(buf, sizeof(buf), "CPU:%.1fms Sim:%.1fms",
                      cpu_time_ms, sim_time_ms);
        s.add(buf);
    }

    // Controls.
    {
        auto& s = out.add_section("CONTROLS");
        s.add("SPC=pause N=step R=reset");
        s.add("1-4=preset F=cam H=hud");
        s.add("Tab=compact/full");
    }
}

// -- Instance generation ----------------------------------------------------

// Layout constants (screen pixels).
static constexpr float k_pixel       = 2.0f;
static constexpr float k_char_pitch  = 6.0f  * k_pixel;
static constexpr float k_line_pitch  = 10.0f * k_pixel;
static constexpr float k_margin      = 8.0f;
static constexpr float k_pad         = 4.0f;
static constexpr float k_section_gap = 6.0f;

// Title color: warm amber.
static constexpr float k_title_r = 0.9f;
static constexpr float k_title_g = 0.7f;
static constexpr float k_title_b = 0.1f;

// Content text color: bright green.
static constexpr float k_text_r = 0.0f;
static constexpr float k_text_g = 0.9f;
static constexpr float k_text_b = 0.35f;

// Background panel color: dark blue-grey.
static constexpr float k_bg_r = 0.02f;
static constexpr float k_bg_g = 0.02f;
static constexpr float k_bg_b = 0.05f;

static int str_len(const char* s, int max_len) {
    int n = 0;
    while (n < max_len && s[n]) ++n;
    return n;
}

// Emit pixel-quads for one text line. Returns updated instance count.
static uint32_t emit_text_line(
    const char* text, int max_len,
    float base_x, float base_y,
    float r, float g, float b,
    OverlayInstance* out, uint32_t n, uint32_t max_count)
{
    float half_px = k_pixel * 0.5f;
    int len = str_len(text, max_len);

    for (int ci = 0; ci < len; ++ci) {
        const uint8_t* glyph = font_glyph(text[ci]);
        if (!glyph) continue;

        float cx = base_x + static_cast<float>(ci) * k_char_pitch;

        for (int row = 0; row < k_font_height; ++row) {
            uint8_t bits = glyph[row];
            if (bits == 0) continue;
            for (int col = 0; col < k_font_width; ++col) {
                if (!(bits & (0x10 >> col))) continue;
                if (n >= max_count) return n;

                float px = cx + static_cast<float>(col) * k_pixel;
                float py = base_y + static_cast<float>(row) * k_pixel;

                out[n].pos_x   = px + half_px;
                out[n].pos_y   = py + half_px;
                out[n].half_sx = half_px;
                out[n].half_sy = half_px;
                out[n].r       = r;
                out[n].g       = g;
                out[n].b       = b;
                out[n].a       = 1.0f;
                out[n].dir_x   = 0.0f;
                out[n].dir_y   = 1.0f;
                ++n;
            }
        }
    }
    return n;
}

uint32_t generate_hud_instances(
    const DebugHudData& data,
    float /*screen_w*/, float /*screen_h*/,
    OverlayInstance* out, uint32_t max_count)
{
    if (data.section_count <= 0 || data.mode == HudMode::Hidden || max_count == 0)
        return 0;

    uint32_t n = 0;
    float cursor_y = k_margin;

    for (int si = 0; si < data.section_count; ++si) {
        const auto& sec = data.sections[si];
        int title_len = str_len(sec.title, DebugHudSection::k_title_len);
        if (sec.line_count <= 0 && title_len == 0) continue;

        // Longest line width (for panel sizing).
        int max_line_len = title_len;
        for (int li = 0; li < sec.line_count; ++li) {
            int ll = str_len(sec.lines[li], DebugHudSection::k_line_len);
            if (ll > max_line_len) max_line_len = ll;
        }

        int total_lines = (title_len > 0 ? 1 : 0) + sec.line_count;
        float char_h = static_cast<float>(k_font_height) * k_pixel;

        float panel_x0 = k_margin - k_pad;
        float panel_y0 = cursor_y - k_pad;
        float panel_x1 = k_margin + static_cast<float>(max_line_len) * k_char_pitch + k_pad;
        float panel_y1 = cursor_y
                       + static_cast<float>(total_lines - 1) * k_line_pitch
                       + char_h + k_pad;

        // Background panel.
        if (n >= max_count) return n;
        out[n].pos_x   = (panel_x0 + panel_x1) * 0.5f;
        out[n].pos_y   = (panel_y0 + panel_y1) * 0.5f;
        out[n].half_sx = (panel_x1 - panel_x0) * 0.5f;
        out[n].half_sy = (panel_y1 - panel_y0) * 0.5f;
        out[n].r       = k_bg_r;
        out[n].g       = k_bg_g;
        out[n].b       = k_bg_b;
        out[n].a       = 1.0f;
        out[n].dir_x   = 0.0f;
        out[n].dir_y   = 1.0f;
        ++n;

        float line_y = cursor_y;

        // Title (amber).
        if (title_len > 0) {
            n = emit_text_line(sec.title, DebugHudSection::k_title_len,
                               k_margin, line_y,
                               k_title_r, k_title_g, k_title_b,
                               out, n, max_count);
            line_y += k_line_pitch;
        }

        // Content lines (green).
        for (int li = 0; li < sec.line_count; ++li) {
            n = emit_text_line(sec.lines[li], DebugHudSection::k_line_len,
                               k_margin, line_y,
                               k_text_r, k_text_g, k_text_b,
                               out, n, max_count);
            line_y += k_line_pitch;
        }

        cursor_y = panel_y1 + k_pad + k_section_gap;
    }

    return n;
}

}  // namespace de
