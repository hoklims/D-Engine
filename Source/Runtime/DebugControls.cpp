#include "Runtime/DebugControls.h"
#include "Runtime/EngineConfig.h"

#include <algorithm>

namespace de {

void DebugControls::apply(const DebugAction& a) {
    if (a.toggle_pause)
        paused = !paused;

    if (a.single_step && paused)
        step_requested = true;

    if (a.reset_scene)
        reset_requested = true;

    if (a.switch_scene >= 0)
        scene_switch = a.switch_scene;

    // Camera.
    if (a.toggle_auto_frame)
        auto_frame = !auto_frame;

    if (!auto_frame) {
        if (a.zoom_delta != 0.0f)
            manual_hw = std::max(1.0f, manual_hw + a.zoom_delta);
        manual_center_x += a.pan_dx;
        manual_center_y += a.pan_dy;
    }
}

bool DebugControls::should_tick() const {
    if (!paused) return true;
    return step_requested;
}

void DebugControls::consume() {
    step_requested  = false;
    reset_requested = false;
    scene_switch    = -1;
}

const char* scene_name(StartScene s) {
    switch (s) {
    case StartScene::Basic:       return "Basic";
    case StartScene::Crowd:       return "Crowd";
    case StartScene::Battlefield: return "Battlefield";
    default:                      return "Unknown";
    }
}

}  // namespace de
