#include "Render/DebugHud.h"
#include "Render/DebugOverlay.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* name) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("FAIL: %s\n", name); }
}

// =================================================================
//  HudMode toggle/cycle helpers
// =================================================================

static void test_hud_mode_toggle() {
    using de::HudMode;

    check(de::toggle_hud_visibility(HudMode::Full)    == HudMode::Hidden, "toggle: Full->Hidden");
    check(de::toggle_hud_visibility(HudMode::Compact) == HudMode::Hidden, "toggle: Compact->Hidden");
    check(de::toggle_hud_visibility(HudMode::Hidden)  == HudMode::Full,   "toggle: Hidden->Full");

    // Double toggle = identity.
    HudMode m = HudMode::Full;
    m = de::toggle_hud_visibility(m);
    m = de::toggle_hud_visibility(m);
    check(m == HudMode::Full, "toggle: double toggle = identity");
}

static void test_hud_mode_cycle() {
    using de::HudMode;

    check(de::next_hud_mode(HudMode::Full)    == HudMode::Compact, "cycle: Full->Compact");
    check(de::next_hud_mode(HudMode::Compact) == HudMode::Full,    "cycle: Compact->Full");
    check(de::next_hud_mode(HudMode::Hidden)  == HudMode::Hidden,  "cycle: Hidden->Hidden (no-op)");

    // Double cycle = identity.
    HudMode m = HudMode::Full;
    m = de::next_hud_mode(m);
    m = de::next_hud_mode(m);
    check(m == HudMode::Full, "cycle: double = identity");
}

// =================================================================
//  DebugHudSection basics
// =================================================================

static void test_section_basics() {
    de::DebugHudSection s;
    s.clear();
    check(s.line_count == 0, "section: clear resets line_count");
    check(s.title[0] == '\0', "section: clear resets title");

    s.set_title("RUNTIME");
    check(std::strcmp(s.title, "RUNTIME") == 0, "section: set_title");

    s.add("Line 1");
    check(s.line_count == 1, "section: add increments");
    check(std::strcmp(s.lines[0], "Line 1") == 0, "section: line content");

    s.add("Line 2");
    check(s.line_count == 2, "section: second add");
}

static void test_section_cap() {
    de::DebugHudSection s;
    s.clear();
    for (int i = 0; i < 20; ++i) s.add("x");
    check(s.line_count == de::DebugHudSection::k_max_lines,
          "section: cap at k_max_lines");
}

static void test_section_truncation() {
    char long_str[128];
    std::memset(long_str, 'B', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    de::DebugHudSection s;
    s.clear();
    s.add(long_str);
    int len = 0;
    while (s.lines[0][len]) ++len;
    check(len <= de::DebugHudSection::k_line_len - 1,
          "section: long line truncated");
}

// =================================================================
//  DebugHudData basics
// =================================================================

static void test_hud_data_basics() {
    de::DebugHudData d;
    d.clear();
    check(d.section_count == 0, "hud: clear resets section_count");

    auto& s1 = d.add_section("RUNTIME");
    check(d.section_count == 1, "hud: add_section increments");
    check(std::strcmp(s1.title, "RUNTIME") == 0, "hud: section title");

    auto& s2 = d.add_section("CROWD");
    check(d.section_count == 2, "hud: second section");
    check(std::strcmp(s2.title, "CROWD") == 0, "hud: second title");
}

static void test_hud_data_cap() {
    de::DebugHudData d;
    d.clear();
    for (int i = 0; i < 10; ++i) d.add_section("X");
    check(d.section_count == de::DebugHudData::k_max_sections,
          "hud: cap at k_max_sections");
}

// =================================================================
//  Extraction: Full mode
// =================================================================

static void test_extract_full() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 100, true,
        200, 200, 180, 20, 0,
        false, true, true, 1.5, 0.8, d);

    check(d.mode == de::HudMode::Full, "full: mode set");
    check(d.section_count == 4, "full: 4 sections");

    // Runtime section.
    check(std::strcmp(d.sections[0].title, "RUNTIME") == 0, "full: s0 title");
    check(std::strstr(d.sections[0].lines[0], "Crowd") != nullptr, "full: scene name");
    check(std::strstr(d.sections[0].lines[1], "RUNNING") != nullptr, "full: running state");
    check(std::strstr(d.sections[0].lines[2], "100") != nullptr, "full: tick");
    check(std::strstr(d.sections[0].lines[3], "auto") != nullptr, "full: camera mode");

    // Crowd section.
    check(std::strcmp(d.sections[1].title, "CROWD") == 0, "full: s1 title");
    check(std::strstr(d.sections[1].lines[0], "200") != nullptr, "full: agent count");
    check(std::strstr(d.sections[1].lines[1], "180") != nullptr, "full: visible count");

    // Budget section.
    check(std::strcmp(d.sections[2].title, "BUDGET") == 0, "full: s2 title");
    check(std::strstr(d.sections[2].lines[0], "OK") != nullptr, "full: budget ok");

    // Controls section.
    check(std::strcmp(d.sections[3].title, "CONTROLS") == 0, "full: s3 title");
    check(std::strstr(d.sections[3].lines[0], "pause") != nullptr, "full: controls hint");
}

static void test_extract_full_paused() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Basic", true, 42, false,
        0, 0, 0, 0, 0,
        false, true, true, 0.5, 0.2, d);

    check(std::strstr(d.sections[0].lines[1], "PAUSED") != nullptr, "full-paused: state");
    check(std::strstr(d.sections[0].lines[3], "manual") != nullptr, "full-paused: camera manual");
}

static void test_extract_full_budget_over() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 0, true,
        100, 100, 100, 0, 0,
        false, true, false, 2.0, 1.0, d);

    check(std::strstr(d.sections[2].lines[0], "OVER") != nullptr, "full: budget over");
}

static void test_extract_full_frame_skipped() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 0, true,
        0, 0, 0, 0, 0,
        true, false, true, 0.0, 0.0, d);

    bool found = false;
    for (int li = 0; li < d.sections[2].line_count; ++li) {
        if (std::strstr(d.sections[2].lines[li], "SKIPPED"))
            found = true;
    }
    check(found, "full: frame skipped shown in budget section");
}

// =================================================================
//  Extraction: Compact mode
// =================================================================

static void test_extract_compact() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Compact,
        "LaneClash", false, 50, true,
        200, 200, 190, 10, 0,
        false, true, true, 1.2, 0.6, d);

    check(d.mode == de::HudMode::Compact, "compact: mode set");
    check(d.section_count == 1, "compact: 1 section");
    check(std::strcmp(d.sections[0].title, "D-ENGINE") == 0, "compact: title");
    check(d.sections[0].line_count >= 2, "compact: >= 2 lines");

    // Line 0: scene + state + tick + cam.
    check(std::strstr(d.sections[0].lines[0], "LaneClash") != nullptr, "compact: scene");
    check(std::strstr(d.sections[0].lines[0], "RUN") != nullptr, "compact: running");
    check(std::strstr(d.sections[0].lines[0], "auto") != nullptr, "compact: cam");

    // Line 1: agents + vis + budget + timing.
    check(std::strstr(d.sections[0].lines[1], "200") != nullptr, "compact: agent count");
    check(std::strstr(d.sections[0].lines[1], "OK") != nullptr, "compact: budget");
}

static void test_extract_compact_frame_skipped() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Compact,
        "Crowd", false, 0, true,
        0, 0, 0, 0, 0,
        true, false, true, 0.0, 0.0, d);

    bool found = false;
    for (int li = 0; li < d.sections[0].line_count; ++li) {
        if (std::strstr(d.sections[0].lines[li], "SKIPPED"))
            found = true;
    }
    check(found, "compact: frame skipped shown");
}

// =================================================================
//  Extraction: Hidden mode
// =================================================================

static void test_extract_hidden() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Hidden,
        "Crowd", false, 100, true,
        200, 200, 200, 0, 0,
        false, true, true, 1.0, 0.5, d);

    check(d.mode == de::HudMode::Hidden, "hidden: mode set");
    check(d.section_count == 0, "hidden: 0 sections");
}

// =================================================================
//  Instance generation: Full mode
// =================================================================

static void test_generate_full() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 42, true,
        100, 100, 100, 0, 0,
        false, true, true, 1.0, 0.5, d);

    de::OverlayInstance buf[de::k_max_overlay_instances];
    uint32_t count = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf, de::k_max_overlay_instances);

    check(count > 4, "gen-full: more than just backgrounds");

    // Should have 4 background panels (one per section).
    // Background panels are dark, large quads.
    int bg_count = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (buf[i].half_sx > 10.0f && buf[i].half_sy > 5.0f &&
            buf[i].r < 0.1f && buf[i].g < 0.1f)
            ++bg_count;
    }
    check(bg_count == 4, "gen-full: 4 background panels");
}

static void test_generate_title_color() {
    de::DebugHudData d;
    d.clear();
    d.mode = de::HudMode::Full;
    auto& s = d.add_section("AB");
    s.add("cd");

    de::OverlayInstance buf[de::k_max_overlay_instances];
    uint32_t count = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf, de::k_max_overlay_instances);

    // After background (index 0), first text pixels should be amber (title).
    bool found_amber = false;
    for (uint32_t i = 1; i < count; ++i) {
        if (buf[i].half_sx < 5.0f) {
            // Small quad = text pixel.
            if (buf[i].r > 0.5f && buf[i].g > 0.3f && buf[i].b < 0.3f) {
                found_amber = true;
                break;
            }
        }
    }
    check(found_amber, "gen: title text is amber");

    // Later pixels should include green (content).
    bool found_green = false;
    for (uint32_t i = 1; i < count; ++i) {
        if (buf[i].half_sx < 5.0f && buf[i].g > 0.5f && buf[i].r < 0.1f) {
            found_green = true;
            break;
        }
    }
    check(found_green, "gen: content text is green");
}

// =================================================================
//  Instance generation: Compact mode
// =================================================================

static void test_generate_compact() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Compact,
        "Crowd", false, 0, true,
        100, 100, 100, 0, 0,
        false, true, true, 1.0, 0.5, d);

    de::OverlayInstance buf[de::k_max_overlay_instances];
    uint32_t count = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf, de::k_max_overlay_instances);

    check(count > 1, "gen-compact: instances generated");

    // Only 1 background panel.
    int bg_count = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (buf[i].half_sx > 10.0f && buf[i].half_sy > 5.0f &&
            buf[i].r < 0.1f && buf[i].g < 0.1f)
            ++bg_count;
    }
    check(bg_count == 1, "gen-compact: 1 background panel");
}

static void test_generate_compact_fewer_than_full() {
    de::DebugHudData full_d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 42, true,
        100, 100, 100, 0, 0,
        false, true, true, 1.0, 0.5, full_d);

    de::DebugHudData compact_d;
    de::extract_debug_hud(
        de::HudMode::Compact,
        "Crowd", false, 42, true,
        100, 100, 100, 0, 0,
        false, true, true, 1.0, 0.5, compact_d);

    de::OverlayInstance buf_full[de::k_max_overlay_instances];
    uint32_t count_full = de::generate_hud_instances(
        full_d, 1280.0f, 720.0f, buf_full, de::k_max_overlay_instances);

    de::OverlayInstance buf_compact[de::k_max_overlay_instances];
    uint32_t count_compact = de::generate_hud_instances(
        compact_d, 1280.0f, 720.0f, buf_compact, de::k_max_overlay_instances);

    check(count_compact < count_full,
          "gen: compact produces fewer instances than full");
}

// =================================================================
//  Instance generation: Hidden mode
// =================================================================

static void test_generate_hidden() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Hidden,
        "Crowd", false, 0, true,
        100, 100, 100, 0, 0,
        false, true, true, 1.0, 0.5, d);

    de::OverlayInstance buf[16];
    uint32_t count = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf, 16);

    check(count == 0, "gen-hidden: 0 instances");
}

// =================================================================
//  Instance generation: empty data
// =================================================================

static void test_generate_empty() {
    de::DebugHudData d;
    d.clear();
    d.mode = de::HudMode::Full;

    de::OverlayInstance buf[16];
    uint32_t count = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf, 16);
    check(count == 0, "gen-empty: 0 sections -> 0 instances");
}

// =================================================================
//  Instance generation: bounds check
// =================================================================

static void test_generate_bounds() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Battlefield", false, 9999, true,
        500, 500, 490, 10, 0,
        false, true, true, 2.0, 1.0, d);

    de::OverlayInstance buf[de::k_max_overlay_instances];
    uint32_t count = de::generate_hud_instances(
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
    check(in_bounds, "gen-bounds: all instances within reasonable screen bounds");
}

// =================================================================
//  Instance generation: cap respected
// =================================================================

static void test_generate_cap() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 42, true,
        100, 100, 100, 0, 0,
        false, true, true, 1.0, 0.5, d);

    de::OverlayInstance buf[4];
    uint32_t count = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf, 4);
    check(count <= 4, "gen-cap: instance cap respected");
}

// =================================================================
//  Instance generation: deterministic
// =================================================================

static void test_generate_deterministic() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", true, 50, true,
        80, 80, 80, 0, 0,
        false, true, true, 1.0, 0.5, d);

    de::OverlayInstance buf1[de::k_max_overlay_instances];
    de::OverlayInstance buf2[de::k_max_overlay_instances];
    uint32_t c1 = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf1, de::k_max_overlay_instances);
    uint32_t c2 = de::generate_hud_instances(
        d, 1280.0f, 720.0f, buf2, de::k_max_overlay_instances);

    check(c1 == c2, "gen-det: deterministic count");
    bool same = (c1 == c2) &&
        (std::memcmp(buf1, buf2, c1 * sizeof(de::OverlayInstance)) == 0);
    check(same, "gen-det: deterministic output");
}

// =================================================================
//  Extraction: timing values present in Full mode
// =================================================================

static void test_extract_full_timing() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 0, true,
        0, 0, 0, 0, 0,
        false, true, true, 16.7, 4.2, d);

    // Budget section should contain timing with CPU: label.
    bool found_timing = false;
    for (int li = 0; li < d.sections[2].line_count; ++li) {
        if (std::strstr(d.sections[2].lines[li], "CPU:") &&
            std::strstr(d.sections[2].lines[li], "Sim:"))
            found_timing = true;
    }
    check(found_timing, "full: timing line present in budget section");
}

// =================================================================
//  Renderer OFF (headless): distinct from SKIPPED
// =================================================================

static void test_extract_full_renderer_off() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Full,
        "Crowd", false, 0, true,
        0, 0, 0, 0, 0,
        false, false, true, 0.0, 0.0, d);

    // Budget section should show "Render: OFF", not "SKIPPED".
    bool found_off = false;
    bool found_skipped = false;
    for (int li = 0; li < d.sections[2].line_count; ++li) {
        if (std::strstr(d.sections[2].lines[li], "Render: OFF"))
            found_off = true;
        if (std::strstr(d.sections[2].lines[li], "SKIPPED"))
            found_skipped = true;
    }
    check(found_off,      "renderer-off full: shows Render: OFF");
    check(!found_skipped, "renderer-off full: no SKIPPED");
}

static void test_extract_compact_renderer_off() {
    de::DebugHudData d;
    de::extract_debug_hud(
        de::HudMode::Compact,
        "Crowd", false, 0, true,
        0, 0, 0, 0, 0,
        false, false, true, 0.0, 0.0, d);

    bool found_off = false;
    bool found_skipped = false;
    for (int li = 0; li < d.sections[0].line_count; ++li) {
        if (std::strstr(d.sections[0].lines[li], "Render: OFF"))
            found_off = true;
        if (std::strstr(d.sections[0].lines[li], "SKIPPED"))
            found_skipped = true;
    }
    check(found_off,      "renderer-off compact: shows Render: OFF");
    check(!found_skipped, "renderer-off compact: no SKIPPED");
}

// =================================================================

int main() {
    test_hud_mode_toggle();
    test_hud_mode_cycle();
    test_section_basics();
    test_section_cap();
    test_section_truncation();
    test_hud_data_basics();
    test_hud_data_cap();
    test_extract_full();
    test_extract_full_paused();
    test_extract_full_budget_over();
    test_extract_full_frame_skipped();
    test_extract_compact();
    test_extract_compact_frame_skipped();
    test_extract_hidden();
    test_generate_full();
    test_generate_title_color();
    test_generate_compact();
    test_generate_compact_fewer_than_full();
    test_generate_hidden();
    test_generate_empty();
    test_generate_bounds();
    test_generate_cap();
    test_generate_deterministic();
    test_extract_full_timing();
    test_extract_full_renderer_off();
    test_extract_compact_renderer_off();

    std::printf("\nDebugHudTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
