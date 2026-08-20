#pragma once

#include "protocol.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace veyra {

enum class TransportState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DEGRADED,
    FAILED,
};

struct TransportMetrics {
    float rttMs{0.0f};
    float jitterMs{0.0f};
    float packetLossPercent{0.0f};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    uint64_t packetsSent{0};
    uint64_t packetsReceived{0};
    uint64_t packetsDropped{0};
    uint32_t currentBitrateBps{0};
};

class Transport {
public:
    using PacketReceivedCallback = std::function<void(const uint8_t* data, size_t size)>;
    using StateChangedCallback = std::function<void(TransportState newState)>;

    virtual ~Transport() = default;

    virtual TransportType GetType() const = 0;
    virtual std::string GetName() const = 0;
    virtual bool Connect(const std::string& endpoint, uint16_t port) = 0;
    virtual int64_t Send(const uint8_t* data, size_t size) = 0;
    virtual void Close() = 0;
    virtual TransportState GetState() const = 0;
    virtual TransportMetrics GetMetrics() const = 0;

    void SetPacketReceivedCallback(PacketReceivedCallback cb) { onPacket_ = cb; }
    void SetStateChangedCallback(StateChangedCallback cb) { onState_ = cb; }

protected:
    PacketReceivedCallback onPacket_;
    StateChangedCallback onState_;
};

} // namespace veyra
