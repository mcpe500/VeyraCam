#pragma once

#include <string>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace veyra {

class TrayIcon {
public:
    using ClickCallback = std::function<void()>;

    TrayIcon();
    ~TrayIcon();

    bool Create(const std::wstring& tooltip, ClickCallback onDoubleClick = nullptr, ClickCallback onExit = nullptr);
    void ShowBalloon(const std::wstring& title, const std::wstring& message);
    void Destroy();

private:
#ifdef _WIN32
    HWND hwnd_{nullptr};
    NOTIFYICONDATAW nid_{};
    ClickCallback onDoubleClick_;
    ClickCallback onExit_;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
};

} // namespace veyra
