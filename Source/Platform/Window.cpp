#include "Platform/Window.h"

namespace de {

static constexpr const char* kWindowClassName = "DEngineWindowClass";

LRESULT CALLBACK Window::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Window* self = reinterpret_cast<Window*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTA*>(lparam);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    case WM_SIZE:
        if (self && wparam != SIZE_MINIMIZED) {
            self->width_  = static_cast<int32_t>(LOWORD(lparam));
            self->height_ = static_cast<int32_t>(HIWORD(lparam));
        }
        return 0;
    case WM_KEYDOWN: {
        if (!self) return 0;
        uint8_t vk = static_cast<uint8_t>(wparam & 0xFF);
        bool was_down = (lparam & (1 << 30)) != 0;
        // Always record in repeat buffer (continuous controls).
        if (self->repeat_count_ < k_max_keys)
            self->repeat_[self->repeat_count_++] = vk;
        // Only record initial press in pressed buffer (one-shot toggles).
        if (!was_down && self->pressed_count_ < k_max_keys)
            self->pressed_[self->pressed_count_++] = vk;
        return 0;
    }
    case WM_CLOSE:
        if (self) self->open_ = false;
        return 0;
    case WM_DESTROY:
        // No PostQuitMessage: engine loop uses open_/running_ flags,
        // not the traditional GetMessage quit path.  PostQuitMessage
        // sets a thread-level quit flag that survives destroy() drains
        // when other messages are still pending, poisoning the next
        // window created on the same thread.
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

bool Window::create(const WindowDesc& desc) {
    // Drain any stale messages left by a previous window lifecycle on
    // this thread (defense-in-depth against leaked quit flags or
    // orphaned posted messages).
    MSG stale;
    while (PeekMessageA(&stale, nullptr, 0, 0, PM_REMOVE)) {}

    HINSTANCE hinstance = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    RECT rect = {0, 0, desc.width, desc.height};
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

    hwnd_ = CreateWindowExA(
        0,
        kWindowClassName,
        desc.title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr, nullptr, hinstance,
        this
    );

    if (!hwnd_) {
        UnregisterClassA(kWindowClassName, hinstance);
        return false;
    }

    width_ = desc.width;
    height_ = desc.height;
    open_ = true;

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    return true;
}

void Window::destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        UnregisterClassA(kWindowClassName, GetModuleHandleA(nullptr));
        // Drain ALL pending messages (not just WM_QUIT).  The old
        // WM_QUIT-only drain could miss the quit flag when other
        // messages were still queued, because PeekMessage only
        // synthesises WM_QUIT once the queue is otherwise empty.
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {}
    }
    open_ = false;
}

bool Window::pump_messages() {
    MSG msg = {};
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            open_ = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return open_;
}

bool Window::is_open() const { return open_; }
HWND Window::handle() const { return hwnd_; }
int32_t Window::width() const { return width_; }
int32_t Window::height() const { return height_; }

void Window::set_title(const char* title) {
    if (hwnd_) SetWindowTextA(hwnd_, title);
}

} // namespace de
