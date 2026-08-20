#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <thread>
#include <random>
#include <cstring>

#include "veyra/protocol.h"
#include "veyra/packetizer.h"
#include "veyra/ring_buffer.h"
#include "veyra/jitter_buffer.h"
#include "veyra/transport_manager.h"
#include "veyra/crypto.h"
#include "veyra/telemetry.h"

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "  Veyra Full-System End-to-End Test Suite" << std::endl;
    std::cout << "=================================================" << std::endl;

    uint32_t sessionId = 0xABCD1234;
    veyra::Packetizer packetizer(sessionId);
    veyra::TelemetryCollector telemetry;

    uint32_t idrRequestsCount = 0;
    std::vector<veyra::VideoFrame> reassembledFrames;
    std::vector<veyra::VideoFrame> jitterPoppedFrames;

    veyra::JitterBufferConfig jbConfig;
    jbConfig.minDelayMs = 5;
    jbConfig.maxDelayMs = 30;
    jbConfig.maxQueueFrames = 50;

    veyra::AdaptiveJitterBuffer jitterBuffer(
        jbConfig,
        [&idrRequestsCount](uint32_t lastGoodId) {
            idrRequestsCount++;
        }
    );

    veyra::FrameReassembler reassembler(
        [&jitterBuffer, &reassembledFrames, &telemetry](veyra::VideoFrame frame) {
            reassembledFrames.push_back(frame);
            jitterBuffer.PushFrame(frame);
            telemetry.RecordBytesReceived(frame.data.size());
        },
        [&idrRequestsCount](uint32_t missingId) {
            idrRequestsCount++;
        }
    );

    std::cout << "[E2E STEP 1] Generating and streaming 60 synthetic 720p frames..." << std::endl;
    for (uint32_t frameId = 1; frameId <= 60; ++frameId) {
        bool isKeyframe = (frameId == 1 || frameId % 30 == 0);
        size_t frameSize = isKeyframe ? 35000 : 8000; // ~35KB for keyframe, ~8KB for P-frame
        std::vector<uint8_t> frameData(frameSize, static_cast<uint8_t>(frameId & 0xFF));

        uint64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();

        auto packets = packetizer.PacketizeFrame(
            frameData.data(), frameData.size(), frameId, nowUs, isKeyframe
        );

        veyra::StageTiming timing;
        timing.captureUs = nowUs;
        timing.encodeUs = nowUs + 2500;
        timing.netSendUs = nowUs + 3500;
        telemetry.RecordFrameTiming(frameId, timing);

        // Transmit packets through reassembler
        for (const auto& pkt : packets) {
            reassembler.PushPacket(pkt.data(), pkt.size());
        }

        // Simulate playback consumption
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto nextF = jitterBuffer.PopNextFrame();
        if (nextF.has_value()) {
            jitterPoppedFrames.push_back(*nextF);
        }
    }

    // Drain remaining jitter buffer frames
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    while (true) {
        auto nextF = jitterBuffer.PopNextFrame();
        if (!nextF.has_value()) break;
        jitterPoppedFrames.push_back(*nextF);
    }

    std::cout << "[E2E STEP 1 CHECK] Reassembled frames count: " << reassembledFrames.size() << " / 60" << std::endl;
    assert(reassembledFrames.size() == 60);

    std::cout << "[E2E STEP 2 CHECK] Jitter buffer successfully popped " << jitterPoppedFrames.size() << " frames" << std::endl;
    assert(!jitterPoppedFrames.empty());
    assert(jitterPoppedFrames.front().frameId == 1);
    assert(jitterPoppedFrames.front().isKeyframe == true);
    std::cout << "[E2E STEP 2 CHECK] First frame ID: " << jitterPoppedFrames.front().frameId << " (Keyframe: " << (jitterPoppedFrames.front().isKeyframe ? "true" : "false") << ") - PASSED" << std::endl;

    std::cout << "[E2E STEP 3] Testing Telemetry and microsecond stage latency calculation..." << std::endl;
    std::string statsJson = telemetry.ToJsonString(
        static_cast<uint8_t>(veyra::TransportType::WIFI_LAN),
        static_cast<uint8_t>(veyra::StreamProfileId::BALANCED)
    );
    std::cout << "Telemetry Output: " << statsJson << std::endl;
    assert(!statsJson.empty());
    std::cout << "[E2E STEP 3 CHECK] Telemetry output generated - PASSED" << std::endl;

    std::cout << "[E2E STEP 4] Testing Cryptographic AEAD roundtrip..." << std::endl;
    auto senderKeys = veyra::SessionCrypto::GenerateKeyPair();
    auto receiverKeys = veyra::SessionCrypto::GenerateKeyPair();

    veyra::SessionCrypto senderCrypto;
    veyra::SessionCrypto receiverCrypto;
    assert(senderCrypto.ComputeSharedKey(receiverKeys.publicKey.data(), senderKeys.privateKey.data()));
    assert(receiverCrypto.ComputeSharedKey(senderKeys.publicKey.data(), receiverKeys.privateKey.data()));

    std::string testPayload = "Veyra Secure Transport Stream - 100% Verified";
    std::vector<uint8_t> plainText(testPayload.begin(), testPayload.end());
    std::vector<uint8_t> cipherText(plainText.size());
    std::array<uint8_t, 16> tag{};

    assert(senderCrypto.Encrypt(plainText.data(), plainText.size(), 1, cipherText.data(), tag.data()));

    std::vector<uint8_t> decrypted(plainText.size());
    assert(receiverCrypto.Decrypt(cipherText.data(), cipherText.size(), tag.data(), 1, decrypted.data()));

    std::string decryptedStr(decrypted.begin(), decrypted.end());
    assert(decryptedStr == testPayload);
    std::cout << "[E2E STEP 4 CHECK] AEAD encrypted & decrypted: \"" << decryptedStr << "\" - PASSED" << std::endl;

    std::cout << "=================================================" << std::endl;
    std::cout << "  ALL FULL-SYSTEM E2E TESTS PASSED 100% SUCCESS  " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
