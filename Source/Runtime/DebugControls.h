#pragma once

#include <cstdint>

namespace de {

enum class StartScene : uint8_t;

// Actions produced by translating raw input into debug commands.
// Testable without Win32 -- just set the flags directly.
struct DebugAction {
    bool toggle_pause   = false;
    bool single_step    = false;
    bool reset_scene    = false;
    int8_t switch_scene  = -1;  // -1 = none, 0 = Basic, 1 = Crowd, 2 = Battlefield
    int8_t switch_preset = -1;  // -1 = none, 0+ = demo preset index

    // Camera.
    bool  toggle_auto_frame = false;
    float zoom_delta        = 0.0f;  // positive = zoom out, negative = zoom in
    float pan_dx            = 0.0f;  // world-space pan
    float pan_dy            = 0.0f;
};

// Runtime state for debug controls.
// Pure data -- no Win32 dependency. Engine reads and mutates this.
struct DebugControls {
    bool    paused          = false;
    bool    step_requested  = false;  // consumed each frame
    bool    reset_requested = false;
    int8_t  scene_switch    = -1;     // pending scene index, -1 = none
    int8_t  preset_switch   = -1;     // pending preset index, -1 = none

    // Camera.
    bool    auto_frame      = true;
    float   manual_center_x = 0.0f;
    float   manual_center_y = 0.0f;
    float   manual_hw       = 50.0f;

    // Apply a DebugAction to this state.
    void apply(const DebugAction& action);

    // Consume one-shot flags (step, reset, scene_switch).
    // Returns true if a sim tick should run this frame.
    bool should_tick() const;

    // Consume the one-shot requests after processing.
    void consume();
};

// Name string for a StartScene value.
const char* scene_name(StartScene s);

}  // namespace de
