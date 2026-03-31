#pragma once

#include "Platform/Window.h"
#include "Runtime/Clock.h"

namespace de {

struct Engine {
    bool init();
    void run();
    void shutdown();

private:
    Window window_;
    Clock clock_;
    bool running_ = false;

    void begin_frame();
    void update();
    void render();
    void end_frame();
};

} // namespace de
