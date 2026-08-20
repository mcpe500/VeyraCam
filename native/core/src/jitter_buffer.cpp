#include "veyra/jitter_buffer.h"
#include <algorithm>

namespace veyra {

AdaptiveJitterBuffer::AdaptiveJitterBuffer(
    const JitterBufferConfig& config,
    IdrRequestCallback onIdrRequest
) : config_(config),
    onIdrRequest_(std::move(onIdrRequest)),
    currentTargetDelayMs_(config.minDelayMs),
    lastEmittedFrameId_(0),
    droppedFrames_(0),
    waitingForInitialKeyframe_(true) {}

void AdaptiveJitterBuffer::PushFrame(VideoFrame frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (waitingForInitialKeyframe_) {
        if (!frame.isKeyframe) {
            if (onIdrRequest_) {
                onIdrRequest_(0);
            }
            return;
        }
        waitingForInitialKeyframe_ = false;
    }

    auto now = std::chrono::steady_clock::now();

    // Check backpressure: if queue is overflowing, drop stale frames
    while (queue_.size() >= config_.maxQueueFrames) {
        // Drop oldest frame
        queue_.pop_front();
        droppedFrames_++;
    }

    // Insert sorted by timestamp
    auto it = std::lower_bound(
        queue_.begin(), queue_.end(), frame.timestampUs,
        [](const std::pair<VideoFrame, std::chrono::steady_clock::time_point>& elem, uint64_t ts) {
            return elem.first.timestampUs < ts;
        }
    );

    queue_.insert(it, {std::move(frame), now});
}

std::optional<VideoFrame> AdaptiveJitterBuffer::PopNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    const auto& oldestEntry = queue_.front();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldestEntry.second).count();

    // If buffered long enough or queue is filling up, pop the frame
    if (elapsedMs >= currentTargetDelayMs_ || queue_.size() >= (config_.maxQueueFrames / 2)) {
        VideoFrame frame = std::move(queue_.front().first);
        queue_.pop_front();
        lastEmittedFrameId_ = frame.frameId;
        return frame;
    }

    return std::nullopt;
}

void AdaptiveJitterBuffer::UpdateNetworkStats(float measuredJitterMs, float packetLossRate) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Target delay = minDelay + 2 * jitter + penalty for loss
    float target = config_.minDelayMs + (2.0f * measuredJitterMs) + (packetLossRate * 20.0f);
    target = std::clamp(target, static_cast<float>(config_.minDelayMs), static_cast<float>(config_.maxDelayMs));
    currentTargetDelayMs_ = static_cast<uint32_t>(target);
}

size_t AdaptiveJitterBuffer::FrameCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void AdaptiveJitterBuffer::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    lastEmittedFrameId_ = 0;
    waitingForInitialKeyframe_ = true;
}

} // namespace veyra
