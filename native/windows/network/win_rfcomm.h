#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace veyra {

struct BluetoothDeviceInfo {
    std::string name;
    std::string address;
    bool isPaired{false};
};

class WinRfcommClient {
public:
    using DataReceivedCallback = std::function<void(const uint8_t* data, size_t size)>;

    WinRfcommClient();
    ~WinRfcommClient();

    std::vector<BluetoothDeviceInfo> DiscoverPairedDevices();
    bool Connect(const std::string& bluetoothAddress, DataReceivedCallback onData = nullptr);
    bool Send(const uint8_t* data, size_t size);
    void Disconnect();

    bool IsConnected() const { return isConnected_; }

private:
    void ReadLoop();

    std::string address_;
    std::atomic<bool> isConnected_{false};
    DataReceivedCallback callback_;
    std::thread readThread_;
    std::mutex sendMutex_;

#ifdef _WIN32
    uintptr_t socket_{static_cast<uintptr_t>(~0)};
#endif
};

} // namespace veyra
