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
    case WM_CLOSE:
        if (self) self->open_ = false;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

bool Window::create(const WindowDesc& desc) {
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
        // Drain any WM_QUIT posted by WM_DESTROY so it does not
        // poison future windows created in the same thread.
        MSG msg;
        while (PeekMessageA(&msg, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {}
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

} // namespace de
