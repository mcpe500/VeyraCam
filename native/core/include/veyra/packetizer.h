#pragma once

#include "protocol.h"
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <mutex>

namespace veyra {

struct VideoFrame {
    uint32_t frameId{0};
    uint64_t timestampUs{0};
    bool isKeyframe{false};
    std::vector<uint8_t> data;
};

class Packetizer {
public:
    Packetizer(uint32_t sessionId);
    ~Packetizer() = default;

    // Fragment a full H.264 frame into packets
    std::vector<std::vector<uint8_t>> PacketizeFrame(
        const uint8_t* frameData,
        size_t frameSize,
        uint32_t frameId,
        uint64_t timestampUs,
        bool isKeyframe
    );

    // Packetize single audio payload
    std::vector<uint8_t> PacketizeAudio(
        const uint8_t* audioData,
        size_t audioSize,
        uint32_t frameId,
        uint64_t timestampUs
    );

private:
    uint32_t sessionId_;
    uint32_t packetSequence_{0};
};

class FrameReassembler {
public:
    using FrameReadyCallback = std::function<void(VideoFrame frame)>;
    using MissingKeyframeCallback = std::function<void(uint32_t frameId)>;

    FrameReassembler(FrameReadyCallback onFrameReady, MissingKeyframeCallback onMissingKeyframe);
    ~FrameReassembler() = default;

    // Process an incoming raw packet
    bool PushPacket(const uint8_t* packetData, size_t packetLength);

    // Reset internal state (e.g. on transport handoff)
    void Reset();

private:
    struct PendingFrame {
        uint32_t frameId{0};
        uint64_t timestampUs{0};
        bool isKeyframe{false};
        uint16_t totalFragments{0};
        uint16_t receivedFragments{0};
        std::map<uint16_t, std::vector<uint8_t>> fragments;
    };

    FrameReadyCallback onFrameReady_;
    MissingKeyframeCallback onMissingKeyframe_;
    std::mutex mutex_;
    std::map<uint32_t, PendingFrame> pendingFrames_;
    uint32_t lastEmittedFrameId_{0};
    bool waitingForKeyframe_{true};
};

} // namespace veyra
