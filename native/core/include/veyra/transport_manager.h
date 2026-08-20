#pragma once

#include "transport.h"
#include "protocol.h"
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>
#include <cstddef>

namespace veyra {

class AutoTransportManager {
public:
    using HandoffCompletedCallback = std::function<void(TransportType oldTransport, TransportType newTransport)>;

    AutoTransportManager();
    ~AutoTransportManager();

    // Register an available transport instance
    void RegisterTransport(std::shared_ptr<Transport> transport);

    // Set auto-selection or pin a specific mode
    void SetPreferredTransportMode(TransportType mode);
    TransportType GetActiveTransportType() const;

    // Send packet via currently active transport
    int64_t SendPacket(const uint8_t* data, size_t size);

    // Evaluate health and initiate seamless handoff if necessary
    bool EvaluateAndHandoff();

    // Trigger explicit handoff to a specific candidate transport
    bool InitiateHandoff(TransportType targetType);

    // Retrieve active link metrics
    TransportMetrics GetActiveMetrics() const;

    void SetHandoffCallback(HandoffCompletedCallback cb) { onHandoffCompleted_ = cb; }

private:
    int CalculateRank(TransportType type, const TransportMetrics& metrics) const;

    mutable std::mutex mutex_;
    std::map<TransportType, std::shared_ptr<Transport>> transports_;
    std::shared_ptr<Transport> activeTransport_;
    TransportType preferredMode_{TransportType::AUTO};
    std::atomic<bool> isHandoffInProgress_{false};
    HandoffCompletedCallback onHandoffCompleted_;
};

} // namespace veyra
