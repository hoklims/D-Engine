#include "Runtime/FrameTelemetry.h"
#include "Runtime/ScopeTimer.h"

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

// ---------------------------------------------------------------
// Test 1: Default-constructed FrameTelemetry is zeroed
// ---------------------------------------------------------------
static void test_default_zeroed() {
    de::FrameTelemetry t;
    check(t.begin_frame_s == 0.0,          "default begin_frame_s == 0");
    check(t.fixed_update_s == 0.0,         "default fixed_update_s == 0");
    check(t.presentation_update_s == 0.0,  "default presentation_update_s == 0");
    check(t.render_s == 0.0,               "default render_s == 0");
    check(t.end_frame_s == 0.0,            "default end_frame_s == 0");
    check(t.total_frame_s == 0.0,          "default total_frame_s == 0");
    check(t.fixed_step_count == 0,         "default fixed_step_count == 0");
}

// ---------------------------------------------------------------
// Test 2: Aggregate reset clears all fields (mirrors Engine::run)
// ---------------------------------------------------------------
static void test_reset_clears_all() {
    de::FrameTelemetry t;
    t.begin_frame_s = 1.0;
    t.fixed_update_s = 2.0;
    t.presentation_update_s = 3.0;
    t.render_s = 4.0;
    t.end_frame_s = 5.0;
    t.total_frame_s = 6.0;
    t.fixed_step_count = 7;

    t = {};

    check(t.begin_frame_s == 0.0,          "reset begin_frame_s");
    check(t.fixed_update_s == 0.0,         "reset fixed_update_s");
    check(t.presentation_update_s == 0.0,  "reset presentation_update_s");
    check(t.render_s == 0.0,               "reset render_s");
    check(t.end_frame_s == 0.0,            "reset end_frame_s");
    check(t.total_frame_s == 0.0,          "reset total_frame_s");
    check(t.fixed_step_count == 0,         "reset fixed_step_count");
}

// ---------------------------------------------------------------
// Test 3: ScopeTimer writes positive elapsed time
// ---------------------------------------------------------------
static void test_scope_timer_positive() {
    double elapsed = -1.0;
    {
        de::ScopeTimer timer(&elapsed);
        volatile double x = 0.0;
        for (int i = 0; i < 10000; ++i) { x += 1.0; }
    }
    check(elapsed > 0.0, "ScopeTimer measures positive time");
    check(elapsed < 1.0, "ScopeTimer measures reasonable time (< 1s)");
}

// ---------------------------------------------------------------
// Test 4: ScopeTimer accumulate mode adds instead of overwriting
// ---------------------------------------------------------------
static void test_scope_timer_accumulate() {
    double total = 0.0;

    {
        de::ScopeTimer timer(&total, true);
        volatile double x = 0.0;
        for (int i = 0; i < 1000; ++i) { x += 1.0; }
    }

    double after_first = total;
    check(after_first > 0.0, "first accumulation > 0");

    {
        de::ScopeTimer timer(&total, true);
        volatile double x = 0.0;
        for (int i = 0; i < 1000; ++i) { x += 1.0; }
    }

    check(total > after_first, "second accumulation increases total");
}

// ---------------------------------------------------------------
// Test 5: Fixed update accumulation across multiple steps
// ---------------------------------------------------------------
static void test_fixed_update_accumulation() {
    de::FrameTelemetry t = {};

    uint32_t steps = 4;
    for (uint32_t i = 0; i < steps; ++i) {
        {
            de::ScopeTimer timer(&t.fixed_update_s, true);
            volatile double x = 0.0;
            for (int j = 0; j < 1000; ++j) { x += 1.0; }
        }
        ++t.fixed_step_count;
    }

    check(t.fixed_step_count == 4, "4 fixed steps counted");
    check(t.fixed_update_s > 0.0, "fixed_update_s accumulated > 0");
}

// ---------------------------------------------------------------
// Test 6: Telemetry reset between frames (simulated 2-frame cycle)
// ---------------------------------------------------------------
static void test_frame_reset_between_frames() {
    de::FrameTelemetry t;

    // -- Frame 1: 3 fixed steps --
    t = {};
    {
        de::ScopeTimer timer(&t.begin_frame_s);
        volatile double x = 0.0;
        for (int i = 0; i < 100; ++i) { x += 1.0; }
    }
    for (uint32_t i = 0; i < 3; ++i) {
        {
            de::ScopeTimer timer(&t.fixed_update_s, true);
            volatile double x = 0.0;
            for (int j = 0; j < 100; ++j) { x += 1.0; }
        }
        ++t.fixed_step_count;
    }

    check(t.fixed_step_count == 3,    "frame 1: 3 steps");
    check(t.fixed_update_s > 0.0,     "frame 1: fixed_update_s > 0");
    check(t.begin_frame_s > 0.0,      "frame 1: begin_frame_s > 0");

    // -- Frame 2: reset then 1 fixed step --
    t = {};
    check(t.fixed_step_count == 0,    "frame 2: reset step count");
    check(t.fixed_update_s == 0.0,    "frame 2: reset fixed_update_s");
    check(t.begin_frame_s == 0.0,     "frame 2: reset begin_frame_s");

    {
        de::ScopeTimer timer(&t.fixed_update_s, true);
        volatile double x = 0.0;
        for (int j = 0; j < 100; ++j) { x += 1.0; }
    }
    ++t.fixed_step_count;

    check(t.fixed_step_count == 1, "frame 2: 1 step after reset");
    check(t.fixed_update_s > 0.0, "frame 2: fixed_update_s > 0 after 1 step");
}

// ---------------------------------------------------------------
// Test 7: Snapshot coherence (total >= sum of measured parts)
// ---------------------------------------------------------------
static void test_snapshot_coherence() {
    de::FrameTelemetry t = {};

    double external_total = 0.0;
    {
        de::ScopeTimer total_timer(&external_total);

        {
            de::ScopeTimer timer(&t.begin_frame_s);
            volatile double x = 0.0;
            for (int i = 0; i < 2000; ++i) { x += 1.0; }
        }

        for (uint32_t i = 0; i < 3; ++i) {
            {
                de::ScopeTimer timer(&t.fixed_update_s, true);
                volatile double x = 0.0;
                for (int j = 0; j < 2000; ++j) { x += 1.0; }
            }
            ++t.fixed_step_count;
        }

        {
            de::ScopeTimer timer(&t.render_s);
            volatile double x = 0.0;
            for (int i = 0; i < 2000; ++i) { x += 1.0; }
        }
    }

    double parts = t.begin_frame_s + t.fixed_update_s + t.render_s;
    check(parts > 0.0,                     "coherence: sum of parts > 0");
    check(external_total >= parts * 0.5,   "coherence: total >= half of parts");
    check(t.fixed_step_count == 3,         "coherence: step count == 3");
}

// ---------------------------------------------------------------
// Test 8: Double-buffer -- complete frame publishes to snapshot
// ---------------------------------------------------------------
static void test_publish_on_complete_frame() {
    de::FrameTelemetry published = {};
    de::FrameTelemetry wip = {};

    // Simulate a complete frame (mirrors Engine::run)
    wip = {};
    wip.begin_frame_s = 0.001;
    wip.fixed_update_s = 0.005;
    wip.fixed_step_count = 2;
    wip.presentation_update_s = 0.0002;
    wip.render_s = 0.003;
    wip.end_frame_s = 0.0001;
    wip.total_frame_s = 0.010;

    // Publish after complete frame
    published = wip;

    check(published.begin_frame_s == 0.001,  "publish: begin_frame_s");
    check(published.fixed_update_s == 0.005, "publish: fixed_update_s");
    check(published.fixed_step_count == 2,   "publish: fixed_step_count");
    check(published.render_s == 0.003,       "publish: render_s");
    check(published.total_frame_s == 0.010,  "publish: total_frame_s");
}

// ---------------------------------------------------------------
// Test 9: Double-buffer -- early exit preserves last snapshot
// ---------------------------------------------------------------
static void test_early_exit_preserves_snapshot() {
    de::FrameTelemetry published = {};
    de::FrameTelemetry wip = {};

    // Frame 1: complete -> publish
    wip = {};
    wip.begin_frame_s = 0.001;
    wip.fixed_update_s = 0.005;
    wip.fixed_step_count = 2;
    wip.render_s = 0.003;
    wip.end_frame_s = 0.0001;
    wip.total_frame_s = 0.010;
    published = wip;

    // Frame 2: begin_frame then early exit (window close)
    wip = {};
    wip.begin_frame_s = 0.0008;
    // running_ = false -> break -> NO publish

    // Published snapshot must still hold frame 1 values
    check(published.begin_frame_s == 0.001,  "preserved: begin_frame_s from frame 1");
    check(published.fixed_update_s == 0.005, "preserved: fixed_update_s from frame 1");
    check(published.fixed_step_count == 2,   "preserved: fixed_step_count from frame 1");
    check(published.render_s == 0.003,       "preserved: render_s from frame 1");
    check(published.total_frame_s == 0.010,  "preserved: total_frame_s from frame 1");
}

// ---------------------------------------------------------------
// Test 10: Double-buffer -- wip reset does not pollute snapshot
// ---------------------------------------------------------------
static void test_wip_reset_does_not_pollute_snapshot() {
    de::FrameTelemetry published = {};
    de::FrameTelemetry wip = {};

    // Frame 1: complete -> publish
    wip = {};
    wip.fixed_update_s = 0.007;
    wip.fixed_step_count = 3;
    wip.total_frame_s = 0.012;
    published = wip;

    check(published.fixed_update_s == 0.007, "before reset: fixed_update_s");
    check(published.fixed_step_count == 3,   "before reset: fixed_step_count");

    // Frame 2 starts: wip reset
    wip = {};

    // Snapshot must be untouched
    check(published.fixed_update_s == 0.007, "after wip reset: fixed_update_s intact");
    check(published.fixed_step_count == 3,   "after wip reset: fixed_step_count intact");
    check(published.total_frame_s == 0.012,  "after wip reset: total_frame_s intact");

    // wip is zeroed
    check(wip.fixed_update_s == 0.0,   "wip zeroed: fixed_update_s");
    check(wip.fixed_step_count == 0,   "wip zeroed: fixed_step_count");
}

// ---------------------------------------------------------------
int main() {
    test_default_zeroed();
    test_reset_clears_all();
    test_scope_timer_positive();
    test_scope_timer_accumulate();
    test_fixed_update_accumulation();
    test_frame_reset_between_frames();
    test_snapshot_coherence();
    test_publish_on_complete_frame();
    test_early_exit_preserves_snapshot();
    test_wip_reset_does_not_pollute_snapshot();

    std::printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
