#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace veyra {

struct UsbDeviceEntry {
    std::string serialNumber;
    std::string model;
    bool isAdbReady{false};
};

class WinUsbManager {
public:
    WinUsbManager();
    ~WinUsbManager();

    std::vector<UsbDeviceEntry> EnumerateConnectedDevices();
    bool SetupAdbPortForward(const std::string& serialNumber, uint16_t localPort = 5150, uint16_t remotePort = 5150);
    bool RemoveAdbPortForward(uint16_t localPort = 5150);
};

} // namespace veyra
