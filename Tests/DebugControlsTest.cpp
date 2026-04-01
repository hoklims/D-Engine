#include "Runtime/DebugControls.h"
#include "Runtime/EngineConfig.h"

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
//  Default state
// =================================================================

static void test_default_state() {
    de::DebugControls dc;
    check(!dc.paused,          "default: not paused");
    check(!dc.step_requested,  "default: no step");
    check(!dc.reset_requested, "default: no reset");
    check(dc.scene_switch == -1, "default: no scene switch");
    check(dc.auto_frame,       "default: auto-frame on");
    check(dc.should_tick(),    "default: should_tick == true");
}

// =================================================================
//  Pause / resume toggle
// =================================================================

static void test_pause_resume() {
    de::DebugControls dc;

    de::DebugAction a = {};
    a.toggle_pause = true;
    dc.apply(a);
    check(dc.paused,           "pause: paused after toggle");
    check(!dc.should_tick(),   "pause: should_tick == false");

    dc.apply(a);
    check(!dc.paused,          "resume: not paused after second toggle");
    check(dc.should_tick(),    "resume: should_tick == true");
}

// =================================================================
//  Single step when paused
// =================================================================

static void test_single_step() {
    de::DebugControls dc;

    // Pause first.
    de::DebugAction pause = {};
    pause.toggle_pause = true;
    dc.apply(pause);
    check(dc.paused, "step: paused");

    // Request single step.
    de::DebugAction step = {};
    step.single_step = true;
    dc.apply(step);
    check(dc.step_requested,   "step: requested");
    check(dc.should_tick(),    "step: should_tick == true while step pending");

    // Consume clears the one-shot.
    dc.consume();
    check(!dc.step_requested,  "step: consumed");
    check(!dc.should_tick(),   "step: should_tick == false after consume");
}

// =================================================================
//  Single step ignored when not paused
// =================================================================

static void test_single_step_while_running() {
    de::DebugControls dc;

    de::DebugAction step = {};
    step.single_step = true;
    dc.apply(step);
    check(!dc.step_requested,  "step-running: ignored when not paused");
    check(dc.should_tick(),    "step-running: still ticks normally");
}

// =================================================================
//  Reset scene
// =================================================================

static void test_reset_scene() {
    de::DebugControls dc;

    de::DebugAction a = {};
    a.reset_scene = true;
    dc.apply(a);
    check(dc.reset_requested, "reset: requested");

    dc.consume();
    check(!dc.reset_requested, "reset: consumed");
}

// =================================================================
//  Scene switch
// =================================================================

static void test_scene_switch() {
    de::DebugControls dc;

    de::DebugAction a = {};
    a.switch_scene = 2;  // Battlefield
    dc.apply(a);
    check(dc.scene_switch == 2, "switch: scene_switch == 2");

    dc.consume();
    check(dc.scene_switch == -1, "switch: consumed");
}

// =================================================================
//  Camera: auto-frame toggle
// =================================================================

static void test_camera_auto_frame_toggle() {
    de::DebugControls dc;
    check(dc.auto_frame, "cam: auto-frame on by default");

    de::DebugAction a = {};
    a.toggle_auto_frame = true;
    dc.apply(a);
    check(!dc.auto_frame, "cam: auto-frame off after toggle");

    dc.apply(a);
    check(dc.auto_frame, "cam: auto-frame on after second toggle");
}

// =================================================================
//  Camera: zoom
// =================================================================

static void test_camera_zoom() {
    de::DebugControls dc;
    dc.auto_frame = false;
    dc.manual_hw  = 50.0f;

    de::DebugAction zoom_out = {};
    zoom_out.zoom_delta = 10.0f;
    dc.apply(zoom_out);
    check(std::fabs(dc.manual_hw - 60.0f) < 0.01f, "zoom: out -> 60");

    de::DebugAction zoom_in = {};
    zoom_in.zoom_delta = -55.0f;
    dc.apply(zoom_in);
    check(dc.manual_hw >= 1.0f, "zoom: clamped >= 1");
}

// =================================================================
//  Camera: zoom ignored in auto-frame mode
// =================================================================

static void test_camera_zoom_ignored_auto() {
    de::DebugControls dc;
    dc.auto_frame = true;
    float hw_before = dc.manual_hw;

    de::DebugAction zoom = {};
    zoom.zoom_delta = 10.0f;
    dc.apply(zoom);
    check(dc.manual_hw == hw_before, "zoom-auto: ignored in auto-frame");
}

// =================================================================
//  Camera: pan
// =================================================================

static void test_camera_pan() {
    de::DebugControls dc;
    dc.auto_frame = false;
    dc.manual_center_x = 0.0f;
    dc.manual_center_y = 0.0f;

    de::DebugAction pan = {};
    pan.pan_dx = 5.0f;
    pan.pan_dy = -3.0f;
    dc.apply(pan);
    check(std::fabs(dc.manual_center_x - 5.0f) < 0.01f,  "pan: dx applied");
    check(std::fabs(dc.manual_center_y + 3.0f) < 0.01f,  "pan: dy applied");
}

// =================================================================
//  Consume is idempotent
// =================================================================

static void test_consume_idempotent() {
    de::DebugControls dc;
    dc.consume();
    dc.consume();
    check(!dc.step_requested,  "consume: idempotent step");
    check(!dc.reset_requested, "consume: idempotent reset");
    check(dc.scene_switch == -1, "consume: idempotent switch");
}

// =================================================================
//  scene_name utility
// =================================================================

static void test_scene_name() {
    check(de::scene_name(de::StartScene::Basic)[0] == 'B',       "scene_name: Basic");
    check(de::scene_name(de::StartScene::Crowd)[0] == 'C',       "scene_name: Crowd");
    check(de::scene_name(de::StartScene::Battlefield)[0] == 'B', "scene_name: Battlefield");
}

// =================================================================
//  Combined sequence: pause -> step -> step -> resume
// =================================================================

static void test_full_sequence() {
    de::DebugControls dc;

    // Pause.
    de::DebugAction pause = {};
    pause.toggle_pause = true;
    dc.apply(pause);
    check(dc.paused,         "seq: paused");
    check(!dc.should_tick(), "seq: no tick when paused");
    dc.consume();

    // Step 1.
    de::DebugAction step = {};
    step.single_step = true;
    dc.apply(step);
    check(dc.should_tick(),  "seq: tick on step 1");
    dc.consume();
    check(!dc.should_tick(), "seq: no tick after consume");

    // Step 2.
    dc.apply(step);
    check(dc.should_tick(),  "seq: tick on step 2");
    dc.consume();

    // Resume.
    dc.apply(pause);
    check(!dc.paused,        "seq: resumed");
    check(dc.should_tick(),  "seq: ticks after resume");
    dc.consume();
}

// =================================================================
//  Repeat resistance: multiple toggle_pause in one action = 1 toggle
// =================================================================

static void test_repeat_toggle_pause() {
    de::DebugControls dc;
    check(!dc.paused, "repeat-pause: starts unpaused");

    // Simulate what happens with a single DebugAction (one frame).
    // Even if toggle_pause is set, it's a single bool -- only one toggle.
    de::DebugAction a = {};
    a.toggle_pause = true;
    dc.apply(a);
    check(dc.paused, "repeat-pause: paused after 1 apply");

    // A second apply (as if a second frame also got the key) toggles again.
    dc.apply(a);
    check(!dc.paused, "repeat-pause: resumed after 2nd apply");

    // But within a single frame, toggle_pause is a bool -- can only be set once.
    // This is the contract: Window gives at most 1 pressed event per key per frame.
}

// =================================================================
//  Repeat resistance: multiple reset in one frame = 1 reset
// =================================================================

static void test_repeat_reset() {
    de::DebugControls dc;

    de::DebugAction a = {};
    a.reset_scene = true;
    dc.apply(a);
    check(dc.reset_requested, "repeat-reset: requested");

    // Apply again (simulating auto-repeat reaching pressed buffer).
    // reset_requested is already true, stays true -- still only 1 reset.
    dc.apply(a);
    check(dc.reset_requested, "repeat-reset: still requested (idempotent)");

    dc.consume();
    check(!dc.reset_requested, "repeat-reset: consumed");
}

// =================================================================
//  Repeat resistance: multiple scene switch = last wins
// =================================================================

static void test_repeat_scene_switch() {
    de::DebugControls dc;

    de::DebugAction a1 = {};
    a1.switch_scene = 0;
    dc.apply(a1);
    check(dc.scene_switch == 0, "repeat-switch: first = 0");

    de::DebugAction a2 = {};
    a2.switch_scene = 2;
    dc.apply(a2);
    check(dc.scene_switch == 2, "repeat-switch: second = 2 (last wins)");

    dc.consume();
}

// =================================================================

int main() {
    test_default_state();
    test_pause_resume();
    test_single_step();
    test_single_step_while_running();
    test_reset_scene();
    test_scene_switch();
    test_camera_auto_frame_toggle();
    test_camera_zoom();
    test_camera_zoom_ignored_auto();
    test_camera_pan();
    test_consume_idempotent();
    test_scene_name();
    test_full_sequence();
    test_repeat_toggle_pause();
    test_repeat_reset();
    test_repeat_scene_switch();

    std::printf("\nDebugControlsTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
