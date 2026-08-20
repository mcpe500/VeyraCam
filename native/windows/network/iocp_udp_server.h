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

    // H-3: once set, only packets from this peer IP are accepted.
    void SetExpectedPeer(const std::string& hostIp);

    uint16_t GetPort() const { return port_; }
    bool IsRunning() const { return isRunning_; }

private:
    void WorkerLoop();

    uint16_t port_{5151};
    std::atomic<bool> isRunning_{false};
    PacketReceivedCallback callback_;
    std::thread workerThread_;

    std::mutex peerMutex_;
    std::string expectedPeer_;
    uint32_t expectedPeerAddr_{0};

#ifdef _WIN32
    SOCKET socket_{INVALID_SOCKET};
#else
    int socket_{-1};
#endif
};

} // namespace veyra
