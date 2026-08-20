#include "veyra/packetizer.h"
#include <cstring>
#include <algorithm>

namespace veyra {

namespace {
// When AEAD is armed, reserve room for the 16-byte tag inside the max payload.
constexpr size_t kAeadTagSize = VEYRA_CRYPTO_TAG_SIZE;
}

// ---------------------------------------------------------------------------
// ReplayFilter
// ---------------------------------------------------------------------------

bool ReplayFilter::Accept(uint32_t sequence) {
    if (!seenAny_) {
        seenAny_ = true;
        highest_ = sequence;
        bitmap_[0] = 1; // bit 0 == highest_
        return true;
    }

    if (sequence > highest_) {
        const uint32_t advance = sequence - highest_;
        if (advance >= kWindow) {
            bitmap_[0] = 1;
        } else {
            bitmap_[0] <<= advance;
            bitmap_[0] |= 1;
        }
        highest_ = sequence;
        return true;
    }

    // sequence <= highest_: inside/behind the window?
    const uint32_t behind = highest_ - sequence;
    if (behind >= kWindow) {
        return false; // too old -> replay
    }
    const uint64_t bit = (uint64_t)1 << behind;
    if (bitmap_[0] & bit) {
        return false; // duplicate -> replay
    }
    bitmap_[0] |= bit;
    return true;
}

// ---------------------------------------------------------------------------
// Packetizer
// ---------------------------------------------------------------------------

Packetizer::Packetizer(uint32_t sessionId)
    : sessionId_(sessionId), packetSequence_(0) {}

void Packetizer::SetCrypto(std::shared_ptr<SessionCrypto> crypto) {
    crypto_ = std::move(crypto);
}

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

    const size_t maxPlaintext = crypto_
        ? (VEYRA_MAX_PAYLOAD_SIZE - kAeadTagSize)
        : VEYRA_MAX_PAYLOAD_SIZE;
    const uint16_t fragmentCount = static_cast<uint16_t>((frameSize + maxPlaintext - 1) / maxPlaintext);

    for (uint16_t i = 0; i < fragmentCount; ++i) {
        size_t offset = i * maxPlaintext;
        size_t currentChunkSize = std::min(maxPlaintext, frameSize - offset);

        uint8_t flags = isKeyframe ? FLAG_KEYFRAME : FLAG_NONE;
        size_t payloadLen = currentChunkSize;
        const uint8_t* payloadSrc = frameData + offset;

        std::vector<uint8_t> cipherBuf;

        if (crypto_) {
            cipherBuf.resize(currentChunkSize + kAeadTagSize);
            payloadLen = currentChunkSize + kAeadTagSize;
            flags |= FLAG_ENCRYPTED;
        }

        // Header is finalized first: it doubles as AEAD additional
        // authenticated data, so any tampering breaks decryption.
        PacketHeader header{};
        header.magic = VEYRA_PROTOCOL_MAGIC;
        header.version = VEYRA_PROTOCOL_VERSION;
        header.flags = flags;
        header.streamId = 0; // Video
        header.sessionId = sessionId_;
        header.frameId = frameId;
        header.sequence = packetSequence_;
        header.fragmentIndex = i;
        header.fragmentCount = fragmentCount;
        header.timestampUs = timestampUs;
        header.payloadLength = static_cast<uint16_t>(payloadLen);

        std::vector<uint8_t> packet(VEYRA_HEADER_SIZE + payloadLen);
        std::memcpy(packet.data(), &header, sizeof(PacketHeader));

        if (crypto_) {
            const uint32_t seq = packetSequence_; // nonce binds this exact packet
            size_t written = crypto_->Encrypt(payloadSrc, currentChunkSize, seq,
                                              packet.data(), VEYRA_HEADER_SIZE,
                                              cipherBuf.data());
            if (written != currentChunkSize + kAeadTagSize) {
                // Encryption failure: never fall back to plaintext silently.
                packetSequence_++;
                continue;
            }
            std::memcpy(packet.data() + VEYRA_HEADER_SIZE, cipherBuf.data(), written);
        } else if (currentChunkSize > 0) {
            std::memcpy(packet.data() + VEYRA_HEADER_SIZE, payloadSrc, currentChunkSize);
        }

        packetSequence_++;
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
    if (audioData && audioSize > VEYRA_MAX_PAYLOAD_SIZE - kAeadTagSize) {
        // Audio frames must fit a single packet in v1 (with AEAD overhead).
        return {};
    }

    uint8_t flags = FLAG_AUDIO;
    size_t payloadLen = audioSize;

    std::vector<uint8_t> cipherBuf;
    if (crypto_) {
        if (audioSize == 0) return {};
        payloadLen = audioSize + kAeadTagSize;
        flags |= FLAG_ENCRYPTED;
    }

    std::vector<uint8_t> packet(VEYRA_HEADER_SIZE + payloadLen);

    PacketHeader header{};
    header.magic = VEYRA_PROTOCOL_MAGIC;
    header.version = VEYRA_PROTOCOL_VERSION;
    header.flags = flags;
    header.streamId = 1; // Audio
    header.sessionId = sessionId_;
    header.frameId = frameId;
    header.sequence = packetSequence_;
    header.fragmentIndex = 0;
    header.fragmentCount = 1;
    header.timestampUs = timestampUs;
    header.payloadLength = static_cast<uint16_t>(payloadLen);

    std::memcpy(packet.data(), &header, sizeof(PacketHeader));

    if (crypto_) {
        std::vector<uint8_t> cipher(audioSize + kAeadTagSize);
        size_t written = crypto_->Encrypt(audioData, audioSize, packetSequence_,
                                          packet.data(), VEYRA_HEADER_SIZE,
                                          cipher.data());
        if (written != audioSize + kAeadTagSize) {
            packetSequence_++;
            return {};
        }
        std::memcpy(packet.data() + VEYRA_HEADER_SIZE, cipher.data(), written);
    } else if (audioData && audioSize > 0) {
        std::memcpy(packet.data() + VEYRA_HEADER_SIZE, audioData, audioSize);
    }

    packetSequence_++;
    return packet;
}

// ---------------------------------------------------------------------------
// FrameReassembler
// ---------------------------------------------------------------------------

FrameReassembler::FrameReassembler(FrameReadyCallback onFrameReady, MissingKeyframeCallback onMissingKeyframe)
    : onFrameReady_(std::move(onFrameReady)),
      onMissingKeyframe_(std::move(onMissingKeyframe)),
      lastEmittedFrameId_(0),
      waitingForKeyframe_(true) {}

void FrameReassembler::SetCrypto(std::shared_ptr<SessionCrypto> crypto) {
    std::lock_guard<std::mutex> lock(mutex_);
    crypto_ = std::move(crypto);
    replayFilter_ = ReplayFilter{};
    sessionLocked_ = false;
}

void FrameReassembler::SetExpectedSessionId(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    expectedSessionId_ = sessionId;
    sessionLocked_ = true;
}

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

    // M-5: only known media stream ids are accepted.
    if (header.streamId != 0 && header.streamId != 1) {
        return false;
    }

    // Per-packet plaintext buffer (decryption happens outside the lock).
    std::vector<uint8_t> plaintext;
    const uint8_t* payloadPtr = packetData + VEYRA_HEADER_SIZE;
    size_t payloadLen = header.payloadLength;

    if (header.flags & FLAG_ENCRYPTED) {
        if (!crypto_) {
            return false; // encrypted packet but no session key -> drop
        }
        plaintext.resize(payloadLen);
        size_t plainLen = crypto_->Decrypt(payloadPtr, payloadLen, header.sequence,
                                           packetData, VEYRA_HEADER_SIZE,
                                           plaintext.data());
        if (plainLen == static_cast<size_t>(-1)) {
            return false; // authentication failed -> drop injected/forged packet
        }
        plaintext.resize(plainLen);
        payloadPtr = plaintext.data();
        payloadLen = plainLen;
    } else if (crypto_) {
        // Downgrade protection: once a session key is armed, plaintext media
        // packets are forbidden.
        return false;
    }

    // Authenticated (or keyless) packet: only now commit the sequence into the
    // replay window so a forged/tampered packet cannot consume a valid one.
    if (!replayFilter_.Accept(header.sequence)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // M-5: session pinning. Adopt-and-lock from the first authenticated packet
    // when no explicit expectation was configured.
    if (!sessionLocked_) {
        expectedSessionId_ = header.sessionId;
        sessionLocked_ = true;
    } else if (header.sessionId != expectedSessionId_) {
        return false; // packet from a different session -> drop
    }

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
        pending.fragments[header.fragmentIndex] = std::vector<uint8_t>(payloadPtr, payloadPtr + payloadLen);
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
    replayFilter_ = ReplayFilter{};
    sessionLocked_ = false;
}

} // namespace veyra
