#include "Runtime/Engine.h"

namespace de {

bool Engine::init() {
    WindowDesc desc;
    desc.title = "D-Engine 2.0";
    desc.width = 1280;
    desc.height = 720;

    if (!window_.create(desc)) return false;

    clock_.init();
    running_ = true;
    return true;
}

void Engine::run() {
    while (running_) {
        begin_frame();
        if (!running_) break;
        update();
        render();
        end_frame();
    }
}

void Engine::shutdown() {
    running_ = false;
    window_.destroy();
}

void Engine::begin_frame() {
    clock_.update();
    if (!window_.pump_messages()) {
        running_ = false;
    }
}

void Engine::update() {
    // Future: fixed-tick simulation
    // Future: ECS update
    // Future: input processing
}

void Engine::render() {
    // Future: DX12 frame submission
    // Future: crowd rendering pipeline
}

void Engine::end_frame() {
    // Future: frame telemetry
    // Future: replay capture
}

} // namespace de
