#pragma once

#include "protocol.h"
#include "transport_manager.h"
#include "packetizer.h"
#include "jitter_buffer.h"
#include "crypto.h"
#include "telemetry.h"
#include <string>
#include <functional>
#include <memory>
#include <atomic>

namespace veyra {

enum class SessionState {
    IDLE,
    PAIRING,
    CONNECTING,
    STREAMING,
    PAUSED,
    DISCONNECTED,
    ERROR_STATE
};

struct StreamConfig {
    uint16_t width{1280};
    uint16_t height{720};
    uint8_t  fps{30};
    uint32_t bitrateBps{2500000};
    StreamProfileId profile{StreamProfileId::BALANCED};
    TransportType preferredTransport{TransportType::AUTO};
    bool enableAudio{true};
    bool enableEncryption{false};
};

struct CameraState {
    float zoom{1.0f};
    int32_t exposure{0};
    bool autoFocus{true};
    float manualFocusDistance{0.0f};
    bool torchOn{false};
    bool useFrontCamera{false};
};

class VeyraSession {
public:
    using StateCallback = std::function<void(SessionState state, const std::string& message)>;
    using FrameReceivedCallback = std::function<void(const VideoFrame& frame)>;
    using StatsCallback = std::function<void(const TelemetryStatsPayload& stats)>;

    VeyraSession();
    ~VeyraSession();

    // Session lifecycle
    bool StartSession(const StreamConfig& config);
    void StopSession();
    void PauseSession();
    void ResumeSession();

    // Pairing: shared crypto context (X25519-derived session keys). The same
    // object is handed to the packetizer (tx) and reassembler (rx).
    std::shared_ptr<SessionCrypto> GetCrypto() { return crypto_; }
    void SetCrypto(std::shared_ptr<SessionCrypto> crypto);

    // Remote camera controls
    void SetZoom(float factor);
    void SetExposure(int32_t step);
    void SetFocus(bool autoFocus, float distance);
    void SetTorch(bool enable);
    void SwitchCamera(bool front);
    void RequestIdr();
    void UpdateBitrate(uint32_t bitrateBps);

    // Transport management
    void SetPreferredTransport(TransportType type);
    void RegisterTransport(std::shared_ptr<Transport> transport);

    // Callbacks
    void SetStateCallback(StateCallback cb) { stateCallback_ = cb; }
    void SetFrameCallback(FrameReceivedCallback cb) { frameCallback_ = cb; }
    void SetStatsCallback(StatsCallback cb) { statsCallback_ = cb; }

    SessionState GetState() const { return state_; }
    StreamConfig GetConfig() const { return config_; }
    CameraState GetCameraState() const { return cameraState_; }
    uint32_t GetSessionId() const { return sessionId_; }
    TelemetryCollector& GetTelemetry() { return telemetry_; }

private:
    void SetState(SessionState newState, const std::string& msg = "");
    void OnAssembledFrameReady(VideoFrame frame);

    uint32_t sessionId_{0};
    std::atomic<SessionState> state_{SessionState::IDLE};
    StreamConfig config_;
    CameraState cameraState_;
    
    std::unique_ptr<AutoTransportManager> transportManager_;
    std::unique_ptr<Packetizer> packetizer_;
    std::unique_ptr<FrameReassembler> reassembler_;
    std::unique_ptr<AdaptiveJitterBuffer> jitterBuffer_;
    std::shared_ptr<SessionCrypto> crypto_;
    TelemetryCollector telemetry_;

    StateCallback stateCallback_;
    FrameReceivedCallback frameCallback_;
    StatsCallback statsCallback_;
};

} // namespace veyra
