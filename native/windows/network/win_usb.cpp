#include "win_usb.h"
#include <iostream>
#include <cstdlib>

namespace veyra {

WinUsbManager::WinUsbManager() = default;

WinUsbManager::~WinUsbManager() = default;

std::vector<UsbDeviceEntry> WinUsbManager::EnumerateConnectedDevices() {
    std::vector<UsbDeviceEntry> list;
    // Query connected Android devices
    return list;
}

bool WinUsbManager::SetupAdbPortForward(const std::string& serialNumber, uint16_t localPort, uint16_t remotePort) {
    std::string cmd = "adb -s " + serialNumber + " forward tcp:" + std::to_string(localPort) + " tcp:" + std::to_string(remotePort);
    int res = system(cmd.c_str());
    return res == 0;
}

bool WinUsbManager::RemoveAdbPortForward(uint16_t localPort) {
    std::string cmd = "adb forward --remove tcp:" + std::to_string(localPort);
    int res = system(cmd.c_str());
    return res == 0;
}

} // namespace veyra
