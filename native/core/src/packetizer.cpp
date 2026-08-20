#include "veyra/packetizer.h"
#include <cstring>
#include <algorithm>

namespace veyra {

Packetizer::Packetizer(uint32_t sessionId)
    : sessionId_(sessionId), packetSequence_(0) {}

std::vector<std::vector<uint8_t>> Packetizer::PacketizeFrame(
    const uint8_t* frameData,
    size_t frameSize,
    uint32_t frameId,
    uint64_t timestampUs,
    bool isKeyframe
) {
    std::vector<std::vector<uint8_t>> packets;
    if (!frameData || frameSize == 0) {
        return packets;
    }

    const size_t maxPayload = VEYRA_MAX_PAYLOAD_SIZE;
    const uint16_t fragmentCount = static_cast<uint16_t>((frameSize + maxPayload - 1) / maxPayload);

    for (uint16_t i = 0; i < fragmentCount; ++i) {
        size_t offset = i * maxPayload;
        size_t currentChunkSize = std::min(maxPayload, frameSize - offset);

        std::vector<uint8_t> packet(VEYRA_HEADER_SIZE + currentChunkSize);

        PacketHeader header{};
        header.magic = VEYRA_PROTOCOL_MAGIC;
        header.version = VEYRA_PROTOCOL_VERSION;
        header.flags = isKeyframe ? FLAG_KEYFRAME : FLAG_NONE;
        header.streamId = 0; // Video
        header.sessionId = sessionId_;
        header.frameId = frameId;
        header.sequence = packetSequence_++;
        header.fragmentIndex = i;
        header.fragmentCount = fragmentCount;
        header.timestampUs = timestampUs;
        header.payloadLength = static_cast<uint16_t>(currentChunkSize);

        std::memcpy(packet.data(), &header, sizeof(PacketHeader));
        std::memcpy(packet.data() + VEYRA_HEADER_SIZE, frameData + offset, currentChunkSize);

        packets.push_back(std::move(packet));
    }

    return packets;
}

std::vector<uint8_t> Packetizer::PacketizeAudio(
    const uint8_t* audioData,
    size_t audioSize,
    uint32_t frameId,
    uint64_t timestampUs
) {
    std::vector<uint8_t> packet(VEYRA_HEADER_SIZE + audioSize);

    PacketHeader header{};
    header.magic = VEYRA_PROTOCOL_MAGIC;
    header.version = VEYRA_PROTOCOL_VERSION;
    header.flags = FLAG_AUDIO;
    header.streamId = 1; // Audio
    header.sessionId = sessionId_;
    header.frameId = frameId;
    header.sequence = packetSequence_++;
    header.fragmentIndex = 0;
    header.fragmentCount = 1;
    header.timestampUs = timestampUs;
    header.payloadLength = static_cast<uint16_t>(audioSize);

    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    if (audioData && audioSize > 0) {
        std::memcpy(packet.data() + VEYRA_HEADER_SIZE, audioData, audioSize);
    }

    return packet;
}

FrameReassembler::FrameReassembler(FrameReadyCallback onFrameReady, MissingKeyframeCallback onMissingKeyframe)
    : onFrameReady_(std::move(onFrameReady)),
      onMissingKeyframe_(std::move(onMissingKeyframe)),
      lastEmittedFrameId_(0),
      waitingForKeyframe_(true) {}

bool FrameReassembler::PushPacket(const uint8_t* packetData, size_t packetLength) {
    if (!packetData || packetLength < VEYRA_HEADER_SIZE) {
        return false;
    }

    PacketHeader header;
    if (!DeserializeHeader(packetData, packetLength, header)) {
        return false;
    }

    if (packetLength < VEYRA_HEADER_SIZE + header.payloadLength) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // If waiting for keyframe and this packet is not keyframe, ignore
    if (waitingForKeyframe_ && !(header.flags & FLAG_KEYFRAME)) {
        if (onMissingKeyframe_) {
            onMissingKeyframe_(header.frameId);
        }
        return false;
    }

    // Ignore frames older than our last emitted frame to prevent jitter inversion
    if (!waitingForKeyframe_ && header.frameId <= lastEmittedFrameId_ && (lastEmittedFrameId_ - header.frameId) < 0x7FFFFFFF) {
        return false;
    }

    auto& pending = pendingFrames_[header.frameId];
    if (pending.frameId == 0) {
        pending.frameId = header.frameId;
        pending.timestampUs = header.timestampUs;
        pending.isKeyframe = (header.flags & FLAG_KEYFRAME) != 0;
        pending.totalFragments = header.fragmentCount;
        pending.receivedFragments = 0;
    }

    if (pending.fragments.find(header.fragmentIndex) == pending.fragments.end()) {
        const uint8_t* payloadPtr = packetData + VEYRA_HEADER_SIZE;
        pending.fragments[header.fragmentIndex] = std::vector<uint8_t>(payloadPtr, payloadPtr + header.payloadLength);
        pending.receivedFragments++;
    }

    // Check if full frame has arrived
    if (pending.receivedFragments == pending.totalFragments && pending.totalFragments > 0) {
        // Reassemble full buffer
        size_t totalSize = 0;
        for (const auto& kv : pending.fragments) {
            totalSize += kv.second.size();
        }

        VideoFrame frame;
        frame.frameId = pending.frameId;
        frame.timestampUs = pending.timestampUs;
        frame.isKeyframe = pending.isKeyframe;
        frame.data.reserve(totalSize);

        for (uint16_t i = 0; i < pending.totalFragments; ++i) {
            auto it = pending.fragments.find(i);
            if (it != pending.fragments.end()) {
                frame.data.insert(frame.data.end(), it->second.begin(), it->second.end());
            }
        }

        lastEmittedFrameId_ = frame.frameId;
        waitingForKeyframe_ = false;

        // Clean up older pending frames
        while (!pendingFrames_.empty() && pendingFrames_.begin()->first <= frame.frameId) {
            pendingFrames_.erase(pendingFrames_.begin());
        }

        if (onFrameReady_) {
            onFrameReady_(std::move(frame));
        }
        return true;
    }

    // Trim old pending frames that took too long (cap buffer size to 16 frames)
    if (pendingFrames_.size() > 16) {
        pendingFrames_.erase(pendingFrames_.begin());
    }

    return true;
}

void FrameReassembler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingFrames_.clear();
    lastEmittedFrameId_ = 0;
    waitingForKeyframe_ = true;
}

} // namespace veyra
