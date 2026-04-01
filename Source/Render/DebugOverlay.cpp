#include "Render/DebugOverlay.h"
#include "Render/BitmapFont.h"

#include <cstdio>
#include <cstring>

namespace de {

// -- DebugOverlayData -------------------------------------------------------

void DebugOverlayData::clear() {
    line_count = 0;
}

void DebugOverlayData::add(const char* text) {
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

// -- Extraction -------------------------------------------------------------

void extract_debug_overlay(
    const char* scene,
    bool        paused,
    uint64_t    tick,
    uint32_t    agent_count,
    uint32_t    instance_count,
    uint32_t    dropped_count,
    bool        frame_skipped,
    bool        within_budget,
    DebugOverlayData& out)
{
    out.clear();
    char buf[DebugOverlayData::k_line_len];

    std::snprintf(buf, sizeof(buf), "Scene: %s", scene);
    out.add(buf);

    out.add(paused ? "State: PAUSED" : "State: RUNNING");

    std::snprintf(buf, sizeof(buf), "Tick: %llu",
                  static_cast<unsigned long long>(tick));
    out.add(buf);

    std::snprintf(buf, sizeof(buf), "Agents: %u", agent_count);
    out.add(buf);

    std::snprintf(buf, sizeof(buf), "Drawn: %u  drop:%u",
                  instance_count, dropped_count);
    out.add(buf);

    if (frame_skipped) {
        out.add("Frame: SKIPPED");
    }

    out.add(within_budget ? "Budget: OK" : "Budget: OVER");
}

// -- Instance generation ----------------------------------------------------

// Layout constants (screen pixels).
static constexpr float k_pixel       = 2.0f;        // size of one font dot
static constexpr float k_char_pitch  = 6.0f  * k_pixel;  // 5 cols + 1 gap
static constexpr float k_line_pitch  = 10.0f * k_pixel;  // 7 rows + 3 gap
static constexpr float k_margin      = 8.0f;
static constexpr float k_pad         = 4.0f;

// Text color: bright green (readable on dark background).
static constexpr float k_text_r = 0.0f;
static constexpr float k_text_g = 0.9f;
static constexpr float k_text_b = 0.35f;

// Background panel color.
static constexpr float k_bg_r = 0.02f;
static constexpr float k_bg_g = 0.02f;
static constexpr float k_bg_b = 0.04f;

static int line_len(const char* s, int max_len) {
    int n = 0;
    while (n < max_len && s[n]) ++n;
    return n;
}

uint32_t generate_overlay_instances(
    const DebugOverlayData& data,
    float /*screen_w*/, float /*screen_h*/,
    OverlayInstance* out, uint32_t max_count)
{
    if (data.line_count <= 0 || max_count == 0) return 0;

    // Find longest line for background panel width.
    int max_len = 0;
    for (int i = 0; i < data.line_count; ++i) {
        int len = line_len(data.lines[i], DebugOverlayData::k_line_len);
        if (len > max_len) max_len = len;
    }

    // Background panel bounds.
    float bg_x0 = k_margin - k_pad;
    float bg_y0 = k_margin - k_pad;
    float bg_x1 = k_margin + static_cast<float>(max_len) * k_char_pitch + k_pad;
    float char_h = static_cast<float>(k_font_height) * k_pixel;
    float bg_y1 = k_margin
                + static_cast<float>(data.line_count - 1) * k_line_pitch
                + char_h + k_pad;

    uint32_t n = 0;

    // Emit background panel (single large quad, drawn first = behind text).
    out[n].pos_x   = (bg_x0 + bg_x1) * 0.5f;
    out[n].pos_y   = (bg_y0 + bg_y1) * 0.5f;
    out[n].half_sx = (bg_x1 - bg_x0) * 0.5f;
    out[n].half_sy = (bg_y1 - bg_y0) * 0.5f;
    out[n].r       = k_bg_r;
    out[n].g       = k_bg_g;
    out[n].b       = k_bg_b;
    out[n].a       = 1.0f;
    out[n].dir_x   = 0.0f;
    out[n].dir_y   = 1.0f;
    ++n;

    // Emit pixel-quads for each lit font dot.
    float half_px = k_pixel * 0.5f;

    for (int li = 0; li < data.line_count; ++li) {
        float base_y = k_margin + static_cast<float>(li) * k_line_pitch;

        int len = line_len(data.lines[li], DebugOverlayData::k_line_len);
        for (int ci = 0; ci < len; ++ci) {
            const uint8_t* glyph = font_glyph(data.lines[li][ci]);
            if (!glyph) continue;

            float base_x = k_margin + static_cast<float>(ci) * k_char_pitch;

            for (int row = 0; row < k_font_height; ++row) {
                uint8_t bits = glyph[row];
                if (bits == 0) continue;   // skip blank rows
                for (int col = 0; col < k_font_width; ++col) {
                    if (!(bits & (0x10 >> col))) continue;
                    if (n >= max_count) return n;

                    float px = base_x + static_cast<float>(col) * k_pixel;
                    float py = base_y + static_cast<float>(row) * k_pixel;

                    out[n].pos_x   = px + half_px;
                    out[n].pos_y   = py + half_px;
                    out[n].half_sx = half_px;
                    out[n].half_sy = half_px;
                    out[n].r       = k_text_r;
                    out[n].g       = k_text_g;
                    out[n].b       = k_text_b;
                    out[n].a       = 1.0f;
                    out[n].dir_x   = 0.0f;
                    out[n].dir_y   = 1.0f;
                    ++n;
                }
            }
        }
    }

    return n;
}

}  // namespace de
