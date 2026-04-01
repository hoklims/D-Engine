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

    // Key input: ring buffer of virtual key codes pressed this frame.
    static constexpr uint32_t k_max_keys = 16;
    uint32_t key_count() const { return key_count_; }
    uint8_t  key_at(uint32_t i) const { return keys_[i]; }
    void     clear_keys() { key_count_ = 0; }

private:
    HWND hwnd_ = nullptr;
    int32_t width_ = 0;
    int32_t height_ = 0;
    bool open_ = false;

    uint8_t  keys_[k_max_keys] = {};
    uint32_t key_count_ = 0;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};

} // namespace de
