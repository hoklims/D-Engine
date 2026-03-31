#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>

namespace de {

struct WindowDesc {
    const char* title = "D-Engine 2.0";
    int32_t width = 1280;
    int32_t height = 720;
};

struct Window {
    bool create(const WindowDesc& desc);
    void destroy();

    bool pump_messages();
    bool is_open() const;

    HWND handle() const;
    int32_t width() const;
    int32_t height() const;

private:
    HWND hwnd_ = nullptr;
    int32_t width_ = 0;
    int32_t height_ = 0;
    bool open_ = false;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};

} // namespace de
