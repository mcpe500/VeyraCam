#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include "veyra/protocol.h"
#include "veyra/packetizer.h"
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
    static ServiceManager& Instance();

    bool Initialize();
    bool ConnectDevice(const std::string& hostIp, uint16_t controlPort = 5150);
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
    ServiceManager();
    ~ServiceManager();

    void OnUdpPacketReceived(const uint8_t* data, size_t size);
    void OnTcpControlMessage(const std::string& message);
    void OnFrameReassembled(VideoFrame frame);

    std::atomic<bool> isStreaming_{false};
    std::mutex mutex_;

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
