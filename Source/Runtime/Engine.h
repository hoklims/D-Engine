#pragma once

#include "Platform/Window.h"
#include "Runtime/Clock.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/FixedStep.h"
#include "Runtime/FrameInfo.h"
#include "Runtime/FrameTelemetry.h"
#include "Runtime/ScopeTimer.h"

namespace de {

struct Engine {
    bool init();
    bool init(const EngineConfig& cfg);
    void run();
    void shutdown();

    const FrameInfo& frame_info() const;
    const FrameTelemetry& frame_telemetry() const;

private:
    EngineConfig config_;
    Window window_;
    Clock clock_;
    FixedStep fixed_step_;
    FrameInfo frame_info_;
    FrameTelemetry telemetry_;        // published snapshot (last complete frame)
    FrameTelemetry wip_telemetry_;    // work-in-progress buffer (current frame)
    bool running_ = false;

    void begin_frame();
    void tick_fixed_steps();
    void update_frame(double alpha);
    void render();
    void end_frame();

    // Hooks for future systems. Override points for simulation and presentation.
    void update_fixed(double step_dt);
    void update_presentation(double alpha);
};

} // namespace de
