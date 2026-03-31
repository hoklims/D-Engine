#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "Runtime/Engine.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    de::Engine engine;

    if (!engine.init()) return 1;
    engine.run();
    engine.shutdown();

    return 0;
}
