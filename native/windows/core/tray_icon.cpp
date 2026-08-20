#include "tray_icon.h"
#include <iostream>

namespace veyra {

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    Destroy();
}

bool TrayIcon::Create(const std::wstring& tooltip, ClickCallback onDoubleClick, ClickCallback onExit) {
#ifdef _WIN32
    onDoubleClick_ = onDoubleClick;
    onExit_ = onExit;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"VeyraTrayClass";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, L"VeyraTrayClass", L"VeyraCore", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;

    memset(&nid_, 0, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_USER + 1;
    nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(nid_.szTip, tooltip.c_str(), 127);

    Shell_NotifyIconW(NIM_ADD, &nid_);
#endif
    return true;
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& message) {
#ifdef _WIN32
    if (!hwnd_) return;
    nid_.uFlags |= NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), 63);
    wcsncpy_s(nid_.szInfo, message.c_str(), 255);
    nid_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
#endif
}

void TrayIcon::Destroy() {
#ifdef _WIN32
    if (hwnd_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
#endif
}

#ifdef _WIN32
LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_USER + 1) {
        if (lParam == WM_LBUTTONDBLCLK) {
            auto* pThis = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (pThis && pThis->onDoubleClick_) pThis->onDoubleClick_();
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif

} // namespace veyra
