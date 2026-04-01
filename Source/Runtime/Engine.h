#pragma once

#include "Platform/Window.h"
#include "Render/RenderFrame.h"
#include "Runtime/Clock.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/FixedStep.h"
#include "Runtime/FrameInfo.h"
#include "Runtime/FrameTelemetry.h"
#include "Runtime/ScopeTimer.h"
#include "Runtime/SimState.h"

namespace de {

struct Renderer;

struct Engine {
    bool init();
    bool init(const EngineConfig& cfg);
    void run();
    void shutdown();

    const FrameInfo& frame_info() const;
    const FrameTelemetry& frame_telemetry() const;
    const SimState& sim_state() const;
    const RenderFrame& render_frame() const;

private:
    EngineConfig config_;
    Window window_;
    Clock clock_;
    FixedStep fixed_step_;
    FrameInfo frame_info_;
    FrameTelemetry telemetry_;        // published snapshot (last complete frame)
    FrameTelemetry wip_telemetry_;    // work-in-progress buffer (current frame)
    SimState sim_;
    bool running_ = false;

    Renderer* renderer_        = nullptr;
    bool      renderer_active_ = false;
    RenderFrame render_frame_;

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
