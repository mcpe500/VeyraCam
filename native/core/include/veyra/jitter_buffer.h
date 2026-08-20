#pragma once

#include "protocol.h"
#include "packetizer.h"
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <optional>
#include <functional>

namespace veyra {

struct JitterBufferConfig {
    uint32_t minDelayMs{20};     // 20ms min delay for stable USB / LAN
    uint32_t maxDelayMs{80};     // 80ms max delay for unstable Wi-Fi
    uint32_t maxQueueFrames{10}; // Cap frame backlog to prevent latency buildup
};

class AdaptiveJitterBuffer {
public:
    using IdrRequestCallback = std::function<void(uint32_t lastGoodFrameId)>;

    explicit AdaptiveJitterBuffer(
        const JitterBufferConfig& config = {},
        IdrRequestCallback onIdrRequest = nullptr
    );
    ~AdaptiveJitterBuffer() = default;

    // Push an assembled video frame into the jitter buffer
    void PushFrame(VideoFrame frame);

    // Pop the next playable frame ready for decoding (if buffer time reached)
    std::optional<VideoFrame> PopNextFrame();

    // Adjust target jitter delay dynamically based on measured network jitter
    void UpdateNetworkStats(float measuredJitterMs, float packetLossRate);

    // Current queue size
    size_t FrameCount() const;

    // Clear all pending frames (used during handoff)
    void Flush();

    // Stats
    uint32_t GetCurrentTargetDelayMs() const { return currentTargetDelayMs_; }
    uint64_t GetDroppedFrames() const { return droppedFrames_; }

private:
    JitterBufferConfig config_;
    IdrRequestCallback onIdrRequest_;
    mutable std::mutex mutex_;
    
    std::deque<std::pair<VideoFrame, std::chrono::steady_clock::time_point>> queue_;
    uint32_t currentTargetDelayMs_{20};
    uint32_t lastEmittedFrameId_{0};
    uint64_t droppedFrames_{0};
    bool waitingForInitialKeyframe_{true};
};

} // namespace veyra
