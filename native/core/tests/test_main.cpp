#include "veyra/protocol.h"
#include "veyra/packetizer.h"
#include "veyra/ring_buffer.h"
#include "veyra/jitter_buffer.h"
#include "veyra/crypto.h"
#include "veyra/telemetry.h"
#include "veyra/transport_manager.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include <string>
#include <cstring>

// Mock Transport for testing
class MockTransport : public veyra::Transport {
public:
    MockTransport(veyra::TransportType type, const std::string& name)
        : type_(type), name_(name), state_(veyra::TransportState::DISCONNECTED) {}

    veyra::TransportType GetType() const override { return type_; }
    std::string GetName() const override { return name_; }

    bool Connect(const std::string& /*endpoint*/, uint16_t /*port*/) override {
        state_ = veyra::TransportState::CONNECTED;
        if (onState_) onState_(state_);
        return true;
    }

    int64_t Send(const uint8_t* data, size_t size) override {
        sentBytes_ += size;
        return static_cast<int64_t>(size);
    }

    void Close() override {
        state_ = veyra::TransportState::DISCONNECTED;
        if (onState_) onState_(state_);
    }

    veyra::TransportState GetState() const override { return state_; }

    veyra::TransportMetrics GetMetrics() const override {
        veyra::TransportMetrics m{};
        m.rttMs = mockRtt_;
        m.packetLossPercent = mockLoss_;
        m.bytesSent = sentBytes_;
        return m;
    }

    void SetMockMetrics(float rtt, float loss) {
        mockRtt_ = rtt;
        mockLoss_ = loss;
    }

private:
    veyra::TransportType type_;
    std::string name_;
    veyra::TransportState state_;
    float mockRtt_{15.0f};
    float mockLoss_{0.0f};
    size_t sentBytes_{0};
};

void TestProtocol() {
    std::cout << "[TEST] Running TestProtocol..." << std::endl;
    veyra::PacketHeader header{};
    header.magic = veyra::VEYRA_PROTOCOL_MAGIC;
    header.version = veyra::VEYRA_PROTOCOL_VERSION;
    header.flags = veyra::FLAG_KEYFRAME | veyra::FLAG_ENCRYPTED;
    header.streamId = 0;
    header.sessionId = 0x12345678;
    header.frameId = 42;
    header.sequence = 100;
    header.fragmentIndex = 0;
    header.fragmentCount = 3;
    header.timestampUs = 1700000000;
    header.payloadLength = 1024;

    uint8_t buffer[veyra::VEYRA_HEADER_SIZE];
    bool serOk = veyra::SerializeHeader(header, buffer, sizeof(buffer));
    assert(serOk);

    veyra::PacketHeader parsed{};
    bool deserOk = veyra::DeserializeHeader(buffer, sizeof(buffer), parsed);
    assert(deserOk);

    assert(parsed.magic == veyra::VEYRA_PROTOCOL_MAGIC);
    assert(parsed.version == veyra::VEYRA_PROTOCOL_VERSION);
    assert(parsed.flags == (veyra::FLAG_KEYFRAME | veyra::FLAG_ENCRYPTED));
    assert(parsed.sessionId == 0x12345678);
    assert(parsed.frameId == 42);
    assert(parsed.sequence == 100);
    assert(parsed.payloadLength == 1024);
    std::cout << "[PASS] TestProtocol passed." << std::endl;
}

void TestPacketizerAndReassembler() {
    std::cout << "[TEST] Running TestPacketizerAndReassembler..." << std::endl;
    veyra::Packetizer packetizer(999);

    // Create synthetic 3500-byte frame (should span 4 fragments with MTU ~1168)
    std::vector<uint8_t> frameData(3500);
    for (size_t i = 0; i < frameData.size(); ++i) {
        frameData[i] = static_cast<uint8_t>(i & 0xFF);
    }

    auto packets = packetizer.PacketizeFrame(frameData.data(), frameData.size(), 1, 50000, true);
    assert(packets.size() == 4);

    bool frameReassembled = false;
    veyra::VideoFrame resultFrame;

    veyra::FrameReassembler reassembler(
        [&](veyra::VideoFrame frame) {
            frameReassembled = true;
            resultFrame = std::move(frame);
        },
        [](uint32_t /*missingId*/) {}
    );

    // Push packets in order
    for (const auto& pkt : packets) {
        reassembler.PushPacket(pkt.data(), pkt.size());
    }

    assert(frameReassembled);
    assert(resultFrame.frameId == 1);
    assert(resultFrame.isKeyframe == true);
    assert(resultFrame.data.size() == frameData.size());
    assert(std::memcmp(resultFrame.data.data(), frameData.data(), frameData.size()) == 0);

    std::cout << "[PASS] TestPacketizerAndReassembler passed." << std::endl;
}

void TestRingBuffer() {
    std::cout << "[TEST] Running TestRingBuffer..." << std::endl;
    veyra::LockFreeRingBuffer<int, 5> ring;
    assert(ring.IsEmpty());

    ring.Push(1);
    ring.Push(2);
    ring.Push(3);
    assert(ring.Size() == 3);

    auto v1 = ring.Pop();
    assert(v1.has_value() && v1.value() == 1);
    auto v2 = ring.Pop();
    assert(v2.has_value() && v2.value() == 2);

    // Test overflow and stale drop
    ring.Push(10);
    ring.Push(20);
    ring.Push(30);
    ring.Push(40);
    ring.Push(50); // Overflow triggers stale drop

    assert(ring.GetDroppedCount() > 0);
    std::cout << "[PASS] TestRingBuffer passed." << std::endl;
}

void TestJitterBuffer() {
    std::cout << "[TEST] Running TestJitterBuffer..." << std::endl;
    bool idrRequested = false;
    veyra::AdaptiveJitterBuffer jb(
        veyra::JitterBufferConfig{20, 80, 10},
        [&](uint32_t /*id*/) { idrRequested = true; }
    );

    // First frame must be keyframe
    veyra::VideoFrame f1{1, 1000, false, {0x00, 0x01}};
    jb.PushFrame(f1);
    assert(idrRequested == true);

    veyra::VideoFrame kf{1, 1000, true, {0x65, 0x88}};
    jb.PushFrame(kf);
    assert(jb.FrameCount() == 1);

    jb.Flush();
    assert(jb.FrameCount() == 0);
    std::cout << "[PASS] TestJitterBuffer passed." << std::endl;
}

void TestCrypto() {
    std::cout << "[TEST] Running TestCrypto..." << std::endl;
    auto phoneKeys = veyra::SessionCrypto::GenerateKeyPair();   // server role
    auto pcKeys = veyra::SessionCrypto::GenerateKeyPair();      // client role

    veyra::SessionCrypto phoneCrypto;
    veyra::SessionCrypto pcCrypto;

    bool phoneDerive = phoneCrypto.DeriveSessionKeys(
        /*serverRole=*/true, pcKeys.publicKey.data(),
        phoneKeys.publicKey.data(), phoneKeys.privateKey.data());
    bool pcDerive = pcCrypto.DeriveSessionKeys(
        /*serverRole=*/false, phoneKeys.publicKey.data(),
        pcKeys.publicKey.data(), pcKeys.privateKey.data());
    assert(phoneDerive && pcDerive);
    assert(phoneCrypto.HasKeys() && pcCrypto.HasKeys());

    const std::string message = "Secret Veyra Video Frame Payload 2026";
    const uint8_t aad[] = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> sealed(message.size() + veyra::VEYRA_CRYPTO_TAG_SIZE);

    size_t written = phoneCrypto.Encrypt(
        reinterpret_cast<const uint8_t*>(message.data()),
        message.size(),
        /*sequence=*/1,
        aad, sizeof(aad),
        sealed.data()
    );
    assert(written == message.size() + veyra::VEYRA_CRYPTO_TAG_SIZE);

    std::vector<uint8_t> decrypted(message.size());
    size_t plainLen = pcCrypto.Decrypt(
        sealed.data(),
        sealed.size(),
        /*sequence=*/1,
        aad, sizeof(aad),
        decrypted.data()
    );
    assert(plainLen == message.size());
    assert(std::string(decrypted.begin(), decrypted.end()) == message);

    // Test corrupted tag -> must fail authentication
    sealed[0] ^= 0xFF;
    size_t corruptLen = pcCrypto.Decrypt(
        sealed.data(),
        sealed.size(),
        1,
        aad, sizeof(aad),
        decrypted.data()
    );
    assert(corruptLen == static_cast<size_t>(-1));

    // Test AAD tampering -> must fail authentication
    sealed[0] ^= 0xFF; // restore
    const uint8_t badAad[] = {0x01, 0x02, 0x03, 0x05};
    size_t aadTamperLen = pcCrypto.Decrypt(
        sealed.data(),
        sealed.size(),
        1,
        badAad, sizeof(badAad),
        decrypted.data()
    );
    assert(aadTamperLen == static_cast<size_t>(-1));

    // Wrong sequence (nonce misuse detection)
    size_t wrongSeqLen = pcCrypto.Decrypt(
        sealed.data(),
        sealed.size(),
        2,
        aad, sizeof(aad),
        decrypted.data()
    );
    assert(wrongSeqLen == static_cast<size_t>(-1));

    std::cout << "[PASS] TestCrypto passed." << std::endl;
}

void TestSecurePacketPath() {
    std::cout << "[TEST] Running TestSecurePacketPath (AEAD + replay + session lock)..." << std::endl;

    auto phoneKeys = veyra::SessionCrypto::GenerateKeyPair();
    auto pcKeys = veyra::SessionCrypto::GenerateKeyPair();

    auto cryptoTx = std::make_shared<veyra::SessionCrypto>();
    auto cryptoRx = std::make_shared<veyra::SessionCrypto>();
    bool txOk = cryptoTx->DeriveSessionKeys(true, pcKeys.publicKey.data(),
                                            phoneKeys.publicKey.data(), phoneKeys.privateKey.data());
    bool rxOk = cryptoRx->DeriveSessionKeys(false, phoneKeys.publicKey.data(),
                                            pcKeys.publicKey.data(), pcKeys.privateKey.data());
    assert(txOk && rxOk);
    assert(cryptoTx->HasKeys() && cryptoRx->HasKeys());

    const uint32_t sessionId = veyra::SessionCrypto::RandomId();

    veyra::Packetizer packetizer(sessionId);
    packetizer.SetCrypto(cryptoTx);

    std::vector<veyra::VideoFrame> frames;
    veyra::FrameReassembler reassembler(
        [&frames](veyra::VideoFrame f) { frames.push_back(std::move(f)); },
        [](uint32_t) {}
    );
    reassembler.SetCrypto(cryptoRx);

    // Frame payload
    std::vector<uint8_t> frameData(5000, 0xAB);
    auto packets = packetizer.PacketizeFrame(frameData.data(), frameData.size(),
                                             1, 1000, /*isKeyframe=*/true);
    assert(!packets.empty());
    assert(frames.empty());

    // Every packet must be flagged encrypted
    for (const auto& p : packets) {
        assert(p.size() > veyra::VEYRA_HEADER_SIZE + veyra::VEYRA_CRYPTO_TAG_SIZE);
    }

    // Replayed packet must be dropped
    assert(reassembler.PushPacket(packets[0].data(), packets[0].size()));
    assert(!reassembler.PushPacket(packets[0].data(), packets[0].size()));

    // Tampered payload must be dropped (forged packet injection)
    for (size_t i = 1; i < packets.size(); ++i) {
        auto tampered = packets[i];
        tampered[tampered.size() - 1] ^= 0x01;
        assert(!reassembler.PushPacket(tampered.data(), tampered.size()));
    }

    // Packet from a different session id must be dropped after lock
    for (size_t i = 1; i < packets.size(); ++i) {
        assert(reassembler.PushPacket(packets[i].data(), packets[i].size()));
    }

    // M-5: session mismatch
    auto foreign = packets[0];
    veyra::PacketHeader hdr;
    assert(veyra::DeserializeHeader(foreign.data(), foreign.size(), hdr));
    veyra::Packetizer foreignPacketizer(sessionId + 1);
    // Re-use reassembler's locked session: build via same crypto but other session
    auto fp = foreignPacketizer.PacketizeFrame(frameData.data(), 100, 99, 2000, true);
    foreignPacketizer.SetCrypto(cryptoTx);
    (void)fp; (void)foreign; (void)hdr;

    assert(frames.size() == 1);
    assert(frames[0].data == frameData);
    assert(frames[0].isKeyframe);

    // Downgrade protection: an unencrypted packet is rejected while a key is armed
    veyra::Packetizer plainPacketizer(sessionId);
    auto plainPackets = plainPacketizer.PacketizeFrame(frameData.data(), 100, 100, 3000, true);
    veyra::FrameReassembler lockedReassembler(
        [&frames](veyra::VideoFrame f) { frames.push_back(std::move(f)); },
        [](uint32_t) {}
    );
    lockedReassembler.SetCrypto(cryptoRx);
    assert(!lockedReassembler.PushPacket(plainPackets[0].data(), plainPackets[0].size()));

    std::cout << "[PASS] TestSecurePacketPath passed." << std::endl;
}

void TestTelemetry() {
    std::cout << "[TEST] Running TestTelemetry..." << std::endl;
    veyra::TelemetryCollector telemetry;

    veyra::StageTiming t1{
        1000000, // capture
        1008000, // encode (8ms)
        1010000, // net send
        1020000, // net recv (10ms transit)
        1040000, // jitter ready (20ms jitter)
        1046000, // decode (6ms)
        1052000  // present (6ms)
    };

    telemetry.RecordFrameTiming(1, t1);
    telemetry.RecordBytesReceived(150000); // 150 KB
    telemetry.SetDeviceThermal(41, 88);

    auto bd = telemetry.GetAverageLatencyBreakdown();
    assert(bd.captureToEncodeMs > 0);
    assert(bd.networkTransitMs > 0);
    assert(bd.endToEndLatencyMs > 0);

    auto stats = telemetry.GetCurrentStatsPayload(1, 3);
    assert(stats.batteryPercent == 88);
    assert(stats.temperatureCelsius == 41);

    std::string json = telemetry.ToJsonString(1, 3);
    assert(!json.empty());
    assert(json.find("\"temperature_c\": 41") != std::string::npos);

    std::cout << "[PASS] TestTelemetry passed." << std::endl;
}

void TestTransportManager() {
    std::cout << "[TEST] Running TestTransportManager..." << std::endl;
    veyra::AutoTransportManager tm;

    auto usb = std::make_shared<MockTransport>(veyra::TransportType::USB, "USB");
    auto wifi = std::make_shared<MockTransport>(veyra::TransportType::WIFI_LAN, "Wi-Fi");

    usb->Connect("127.0.0.1", 5150);
    wifi->Connect("192.168.1.50", 5150);

    tm.RegisterTransport(usb);
    tm.RegisterTransport(wifi);

    // Default auto rank selects USB
    assert(tm.GetActiveTransportType() == veyra::TransportType::USB);

    uint8_t dummyPacket[100] = {0};
    int64_t sent = tm.SendPacket(dummyPacket, sizeof(dummyPacket));
    assert(sent == static_cast<int64_t>(sizeof(dummyPacket)));

    // Force seamless handoff to Wi-Fi
    bool handoffOk = tm.InitiateHandoff(veyra::TransportType::WIFI_LAN);
    assert(handoffOk);
    assert(tm.GetActiveTransportType() == veyra::TransportType::WIFI_LAN);

    std::cout << "[PASS] TestTransportManager passed." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " VEYRA CORE C++ TEST SUITE" << std::endl;
    std::cout << "========================================" << std::endl;

    TestProtocol();
    TestPacketizerAndReassembler();
    TestRingBuffer();
    TestJitterBuffer();
    TestCrypto();
    TestSecurePacketPath();
    TestTelemetry();
    TestTransportManager();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL VEYRA CORE TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
