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
    void set_title(const char* title);

    // Key input: two ring buffers of virtual key codes per frame.
    //   pressed_*  = initial key-down only (bit 30 == 0), for one-shot toggles
    //   repeat_*   = all key-downs including auto-repeat, for continuous controls
    static constexpr uint32_t k_max_keys = 16;

    uint32_t pressed_count() const { return pressed_count_; }
    uint8_t  pressed_at(uint32_t i) const { return pressed_[i]; }

    uint32_t repeat_count() const { return repeat_count_; }
    uint8_t  repeat_at(uint32_t i) const { return repeat_[i]; }

    void clear_keys() { pressed_count_ = 0; repeat_count_ = 0; }

private:
    HWND hwnd_ = nullptr;
    int32_t width_ = 0;
    int32_t height_ = 0;
    bool open_ = false;

    uint8_t  pressed_[k_max_keys] = {};
    uint32_t pressed_count_ = 0;
    uint8_t  repeat_[k_max_keys] = {};
    uint32_t repeat_count_ = 0;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};

} // namespace de
