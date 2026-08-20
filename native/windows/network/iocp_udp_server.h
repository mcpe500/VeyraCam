#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "veyra/protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

namespace veyra {

class IocpUdpServer {
public:
    using PacketReceivedCallback = std::function<void(const uint8_t* data, size_t size)>;

    IocpUdpServer(uint16_t port = 5151);
    ~IocpUdpServer();

    bool Start(PacketReceivedCallback onPacketReceived);
    void Stop();

    uint16_t GetPort() const { return port_; }
    bool IsRunning() const { return isRunning_; }

private:
    void WorkerLoop();

    uint16_t port_{5151};
    std::atomic<bool> isRunning_{false};
    PacketReceivedCallback callback_;
    std::thread workerThread_;

#ifdef _WIN32
    SOCKET socket_{INVALID_SOCKET};
    HANDLE iocp_{nullptr};
#else
    int socket_{-1};
#endif
};

} // namespace veyra
