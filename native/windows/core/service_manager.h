#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include "veyra/protocol.h"
#include "veyra/packetizer.h"
#include "veyra/crypto.h"
#include "veyra/jitter_buffer.h"
#include "veyra/telemetry.h"
#include "../network/iocp_udp_server.h"
#include "../network/tcp_control_server.h"
#include "../decoder/mf_decoder.h"
#include "../renderer/d3d11_renderer.h"
#include "../virtual_camera/mf/mf_virtual_camera.h"
#include "../virtual_camera/dshow/dshow_filter.h"

namespace veyra {

class ServiceManager {
public:
    // C-2: status events surfaced to the FFI layer.
    using StatusCallback = std::function<void(const char* status)>;

    static ServiceManager& Instance();

    bool Initialize();
    bool ConnectDevice(const std::string& hostIp, uint16_t controlPort,
                       const std::string& pin, StatusCallback onStatus);
    void DisconnectDevice();

    void SetZoom(float zoom);
    void SetExposure(int32_t exposure);
    void SetFocus(bool autoFocus, float distance);
    void SetTorch(bool enabled);
    void SwitchCamera(bool front);
    void RequestIdr();

    uint64_t GetSharedTextureHandle() const;
    std::string GetTelemetryJson();
    bool IsStreaming() const { return isStreaming_; }
    void Shutdown();

private:
    enum class PairingState {
        DISCONNECTED,
        AWAITING_CHALLENGE,
        AWAITING_PAIRING_OK,
        PAIRED
    };

    ServiceManager();
    ~ServiceManager();

    void OnUdpPacketReceived(const uint8_t* data, size_t size);
    void OnTcpControlMessage(const std::string& message);
    void OnFrameReassembled(VideoFrame frame);

    void HandlePairingChallenge(const std::string& serverPubKeyB64, uint32_t sessionId);
    void HandlePairingResult(const std::string& token, const std::string& errorReason);
    std::string BuildControlMessage(const std::string& type, const std::string& payloadJson) const;

    std::atomic<bool> isStreaming_{false};
    std::mutex mutex_;

    PairingState pairingState_{PairingState::DISCONNECTED};
    std::string pin_;
    std::string token_;
    std::string hostIp_;
    std::shared_ptr<veyra::SessionCrypto> crypto_;
    StatusCallback statusCallback_;

    std::unique_ptr<IocpUdpServer> udpServer_;
    std::unique_ptr<TcpControlClient> tcpClient_;
    std::unique_ptr<FrameReassembler> reassembler_;
    std::unique_ptr<AdaptiveJitterBuffer> jitterBuffer_;
    std::unique_ptr<MFDecoder> decoder_;
    std::unique_ptr<D3D11Renderer> renderer_;
    std::unique_ptr<MFVirtualCameraManager> virtualCamera_;
    std::unique_ptr<DShowFilter> dshowFilter_;
    TelemetryCollector telemetry_;
};

} // namespace veyra
