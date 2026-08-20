#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "veyra/protocol.h"

namespace veyra {

class TcpControlClient {
public:
    using MessageReceivedCallback = std::function<void(const std::string& jsonMessage)>;

    TcpControlClient();
    ~TcpControlClient();

    bool Connect(const std::string& host, uint16_t port = 5150, MessageReceivedCallback onMessage = nullptr);
    bool SendControlMessage(const std::string& jsonMessage);
    void Disconnect();

    bool IsConnected() const { return isConnected_; }

private:
    void ReadLoop();

    std::string host_;
    uint16_t port_{5150};
    std::atomic<bool> isConnected_{false};
    MessageReceivedCallback callback_;
    std::thread readThread_;
    std::mutex sendMutex_;

#ifdef _WIN32
    uintptr_t socket_{static_cast<uintptr_t>(~0)};
#else
    int socket_{-1};
#endif
};

} // namespace veyra
