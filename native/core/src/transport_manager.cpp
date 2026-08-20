#include "veyra/transport_manager.h"
#include <algorithm>

namespace veyra {

AutoTransportManager::AutoTransportManager()
    : preferredMode_(TransportType::AUTO),
      isHandoffInProgress_(false) {}

AutoTransportManager::~AutoTransportManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : transports_) {
        if (pair.second) {
            pair.second->Close();
        }
    }
}

void AutoTransportManager::RegisterTransport(std::shared_ptr<Transport> transport) {
    if (!transport) return;
    std::lock_guard<std::mutex> lock(mutex_);
    transports_[transport->GetType()] = transport;

    if (!activeTransport_ || activeTransport_->GetState() != TransportState::CONNECTED) {
        if (transport->GetState() == TransportState::CONNECTED) {
            activeTransport_ = transport;
        }
    }
}

void AutoTransportManager::SetPreferredTransportMode(TransportType mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    preferredMode_ = mode;
    if (mode != TransportType::AUTO) {
        auto it = transports_.find(mode);
        if (it != transports_.end() && it->second->GetState() == TransportState::CONNECTED) {
            activeTransport_ = it->second;
        }
    }
}

TransportType AutoTransportManager::GetActiveTransportType() const {
    if (activeTransport_) {
        return activeTransport_->GetType();
    }
    return TransportType::AUTO;
}

ssize_t AutoTransportManager::SendPacket(const uint8_t* data, size_t size) {
    std::shared_ptr<Transport> current;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current = activeTransport_;
    }

    if (current && current->GetState() == TransportState::CONNECTED) {
        return current->Send(data, size);
    }
    return -1;
}

int AutoTransportManager::CalculateRank(TransportType type, const TransportMetrics& metrics) const {
    // Lower score is better
    int baseScore = 0;
    switch (type) {
        case TransportType::USB: baseScore = 10; break;
        case TransportType::WIFI_LAN: baseScore = 20; break;
        case TransportType::WIFI_DIRECT: baseScore = 30; break;
        case TransportType::BLUETOOTH: baseScore = 50; break;
        default: baseScore = 100; break;
    }

    // Penalties for latency & packet loss
    int penalty = static_cast<int>(metrics.rttMs) + static_cast<int>(metrics.packetLossPercent * 5.0f);
    return baseScore + penalty;
}

bool AutoTransportManager::EvaluateAndHandoff() {
    if (isHandoffInProgress_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (preferredMode_ != TransportType::AUTO) {
        return false;
    }

    std::shared_ptr<Transport> bestCandidate = nullptr;
    int bestScore = 99999;

    for (const auto& kv : transports_) {
        if (kv.second && kv.second->GetState() == TransportState::CONNECTED) {
            int score = CalculateRank(kv.first, kv.second->GetMetrics());
            if (score < bestScore) {
                bestScore = score;
                bestCandidate = kv.second;
            }
        }
    }

    if (bestCandidate && bestCandidate != activeTransport_) {
        TransportType oldType = activeTransport_ ? activeTransport_->GetType() : TransportType::AUTO;
        TransportType newType = bestCandidate->GetType();

        isHandoffInProgress_.store(true);
        activeTransport_ = bestCandidate;
        isHandoffInProgress_.store(false);

        if (onHandoffCompleted_) {
            onHandoffCompleted_(oldType, newType);
        }
        return true;
    }

    return false;
}

bool AutoTransportManager::InitiateHandoff(TransportType targetType) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = transports_.find(targetType);
    if (it != transports_.end() && it->second && it->second->GetState() == TransportState::CONNECTED) {
        TransportType oldType = activeTransport_ ? activeTransport_->GetType() : TransportType::AUTO;
        activeTransport_ = it->second;
        if (onHandoffCompleted_) {
            onHandoffCompleted_(oldType, targetType);
        }
        return true;
    }
    return false;
}

TransportMetrics AutoTransportManager::GetActiveMetrics() const {
    if (activeTransport_) {
        return activeTransport_->GetMetrics();
    }
    return TransportMetrics{};
}

} // namespace veyra
