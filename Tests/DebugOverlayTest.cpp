#include "Render/DebugOverlay.h"
#include "Render/BitmapFont.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* name) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("FAIL: %s\n", name); }
}

int main() {
    // -- DebugOverlayData basics --

    {
        de::DebugOverlayData d;
        d.clear();
        check(d.line_count == 0, "initial line_count is 0");

        d.add("Hello");
        check(d.line_count == 1, "add increments line_count");
        check(std::strcmp(d.lines[0], "Hello") == 0, "line content matches");

        d.add("World");
        check(d.line_count == 2, "second add");

        d.clear();
        check(d.line_count == 0, "clear resets line_count");
    }

    // -- Max lines cap --

    {
        de::DebugOverlayData d;
        d.clear();
        for (int i = 0; i < 20; ++i) d.add("x");
        check(d.line_count == de::DebugOverlayData::k_max_lines,
              "line_count capped at k_max_lines");
    }

    // -- Long line truncation --

    {
        char long_str[128];
        std::memset(long_str, 'A', sizeof(long_str) - 1);
        long_str[sizeof(long_str) - 1] = '\0';

        de::DebugOverlayData d;
        d.clear();
        d.add(long_str);
        int len = 0;
        while (d.lines[0][len]) ++len;
        check(len <= de::DebugOverlayData::k_line_len - 1,
              "long line is truncated");
    }

    // -- Extraction: running scene --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Crowd", false, 100, 200, 200, 0, 0,
                                  false, true, d);

        check(d.line_count >= 5, "extraction produces >= 5 lines");
        check(std::strstr(d.lines[0], "Crowd") != nullptr,
              "scene name present");
        check(std::strstr(d.lines[1], "RUNNING") != nullptr,
              "running state");
        check(std::strstr(d.lines[2], "100") != nullptr,
              "tick count present");
        check(std::strstr(d.lines[3], "200") != nullptr,
              "agent count present");
    }

    // -- Extraction: paused --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Basic", true, 0, 0, 0, 0, 0,
                                  false, true, d);
        check(std::strstr(d.lines[1], "PAUSED") != nullptr,
              "paused state shown");
    }

    // -- Extraction: frame skipped adds a line --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Crowd", false, 0, 0, 0, 0, 0,
                                  true, true, d);
        bool found = false;
        for (int i = 0; i < d.line_count; ++i)
            if (std::strstr(d.lines[i], "SKIPPED")) found = true;
        check(found, "frame_skipped line present");
    }

    // -- Extraction: budget over --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Crowd", false, 0, 0, 0, 0, 0,
                                  false, false, d);
        bool found = false;
        for (int i = 0; i < d.line_count; ++i)
            if (std::strstr(d.lines[i], "OVER")) found = true;
        check(found, "budget OVER line present");
    }

    // -- Extraction: budget OK --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Crowd", false, 0, 0, 0, 0,
                                  false, true, d);
        bool found = false;
        for (int i = 0; i < d.line_count; ++i)
            if (std::strstr(d.lines[i], "OK")) found = true;
        check(found, "budget OK line present");
    }

    // -- Instance generation: basic --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Crowd", false, 42, 100, 100, 0, 0,
                                  false, true, d);

        de::OverlayInstance buf[de::k_max_overlay_instances];
        uint32_t count = de::generate_overlay_instances(
            d, 1280.0f, 720.0f, buf, de::k_max_overlay_instances);

        check(count > 1, "generated more than just background");

        // First instance = background panel.
        check(buf[0].half_sx > 10.0f, "bg panel has width");
        check(buf[0].half_sy > 10.0f, "bg panel has height");
        check(buf[0].r < 0.1f && buf[0].g < 0.1f, "bg panel is dark");

        // Text instances are green.
        if (count > 1) {
            check(buf[1].g > 0.5f, "text pixel is green");
            check(buf[1].r < 0.1f, "text pixel has low red");
        }
    }

    // -- Instance generation: positions in screen bounds --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Battlefield", false, 9999, 500, 490, 0, 10,
                                  false, true, d);

        de::OverlayInstance buf[de::k_max_overlay_instances];
        uint32_t count = de::generate_overlay_instances(
            d, 1280.0f, 720.0f, buf, de::k_max_overlay_instances);

        bool in_bounds = true;
        for (uint32_t i = 0; i < count; ++i) {
            float x = buf[i].pos_x;
            float y = buf[i].pos_y;
            if (x < -50.0f || x > 1330.0f || y < -50.0f || y > 770.0f) {
                in_bounds = false;
                break;
            }
        }
        check(in_bounds, "all instances within reasonable bounds");
    }

    // -- Instance generation: empty overlay --

    {
        de::DebugOverlayData d;
        d.clear();

        de::OverlayInstance buf[16];
        uint32_t count = de::generate_overlay_instances(
            d, 1280.0f, 720.0f, buf, 16);
        check(count == 0, "empty overlay produces 0 instances");
    }

    // -- Instance generation: cap respected --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Crowd", false, 42, 100, 100, 0, 0,
                                  false, true, d);

        de::OverlayInstance buf[4];
        uint32_t count = de::generate_overlay_instances(
            d, 1280.0f, 720.0f, buf, 4);
        check(count <= 4, "instance cap respected");
    }

    // -- BitmapFont glyph lookup --

    {
        check(de::font_glyph('A') != nullptr, "glyph A exists");
        check(de::font_glyph('Z') != nullptr, "glyph Z exists");
        check(de::font_glyph('0') != nullptr, "glyph 0 exists");
        check(de::font_glyph('9') != nullptr, "glyph 9 exists");
        check(de::font_glyph(':') != nullptr, "glyph : exists");
        check(de::font_glyph(' ') != nullptr, "glyph space exists");
        check(de::font_glyph('~') != nullptr, "glyph ~ exists");
        check(de::font_glyph(1)   == nullptr, "control char -> null");
        check(de::font_glyph(127) == nullptr, "DEL -> null");
    }

    // -- BitmapFont: glyphs have pixels --

    {
        const uint8_t* ga = de::font_glyph('A');
        int pixels_a = 0;
        for (int r = 0; r < de::k_font_height; ++r)
            for (int c = 0; c < de::k_font_width; ++c)
                if (ga[r] & (0x10 >> c)) ++pixels_a;
        check(pixels_a > 5, "glyph A has lit pixels");

        const uint8_t* gs = de::font_glyph(' ');
        int pixels_space = 0;
        for (int r = 0; r < de::k_font_height; ++r)
            for (int c = 0; c < de::k_font_width; ++c)
                if (gs[r] & (0x10 >> c)) ++pixels_space;
        check(pixels_space == 0, "space glyph has no pixels");
    }

    // -- Instance generation: deterministic --

    {
        de::DebugOverlayData d;
        de::extract_debug_overlay("Crowd", true, 50, 80, 80, 0, 0,
                                  false, true, d);

        de::OverlayInstance buf1[de::k_max_overlay_instances];
        de::OverlayInstance buf2[de::k_max_overlay_instances];
        uint32_t c1 = de::generate_overlay_instances(
            d, 1280.0f, 720.0f, buf1, de::k_max_overlay_instances);
        uint32_t c2 = de::generate_overlay_instances(
            d, 1280.0f, 720.0f, buf2, de::k_max_overlay_instances);

        check(c1 == c2, "deterministic count");
        bool same = (c1 == c2) &&
            (std::memcmp(buf1, buf2, c1 * sizeof(de::OverlayInstance)) == 0);
        check(same, "deterministic output");
    }

    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
