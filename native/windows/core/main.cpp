#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "service_manager.h"
#include "tray_icon.h"

#ifdef _WIN32
#include <windows.h>
#endif

static std::atomic<bool> g_running{true};

void SignalHandler(int signum) {
    std::cout << "\n[VeyraCore] Signal received (" << signum << "), shutting down..." << std::endl;
    g_running = false;
}

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
#else
int main(int argc, char* argv[]) {
#endif
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    std::cout << "=============================================" << std::endl;
    std::cout << "  Veyra Core — Native Background Media Engine" << std::endl;
    std::cout << "  Veyra Camera Virtual Driver & Network Service" << std::endl;
    std::cout << "=============================================" << std::endl;

    auto& service = veyra::ServiceManager::Instance();
    if (!service.Initialize()) {
        std::cerr << "[VeyraCore] Failed to initialize ServiceManager" << std::endl;
        return 1;
    }

    veyra::TrayIcon tray;
    tray.Create(L"Veyra Camera Core Service", []() {
        std::cout << "[VeyraCore] Tray double clicked: launching VeyraLink UI..." << std::endl;
    }, []() {
        g_running = false;
    });

    std::cout << "[VeyraCore] Service running in tray. Idle memory target <50 MB." << std::endl;

#ifdef _WIN32
    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
#else
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif

    tray.Destroy();
    service.Shutdown();
    std::cout << "[VeyraCore] Exited cleanly." << std::endl;
    return 0;
}
