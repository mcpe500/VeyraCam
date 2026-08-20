#include "service_manager.h"
#include <iostream>
#include <chrono>

namespace veyra {

ServiceManager& ServiceManager::Instance() {
    static ServiceManager instance;
    return instance;
}

ServiceManager::ServiceManager() = default;

ServiceManager::~ServiceManager() {
    Shutdown();
}

bool ServiceManager::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Initialize Renderer (Direct3D 11 NV12 shared texture)
    renderer_ = std::make_unique<D3D11Renderer>();
    renderer_->Initialize(1280, 720);

    // 2. Initialize Virtual Camera (Win11 IMFVirtualCamera + Win10 DShow fallback)
    virtualCamera_ = std::make_unique<MFVirtualCameraManager>();
    if (!virtualCamera_->RegisterVirtualCamera(L"Veyra Camera")) {
        dshowFilter_ = std::make_unique<DShowFilter>();
        dshowFilter_->Initialize(1280, 720, 30);
    }
    virtualCamera_->Start();

    // 3. Initialize Decoder (Media Foundation Hardware H.264)
    decoder_ = std::make_unique<MFDecoder>();
    decoder_->Initialize(1280, 720, true);
    decoder_->SetFrameCallback([this](const DecodedFrameNV12& frame) {
        // Single decode -> share simultaneously to D3D11 renderer & Virtual Camera
        if (renderer_) renderer_->RenderFrame(frame);
        if (virtualCamera_) virtualCamera_->PushFrame(frame);
        if (dshowFilter_) dshowFilter_->PushFrame(frame);
    });

    // 4. Initialize Jitter Buffer
    JitterBufferConfig jbConfig;
    jbConfig.minDelayMs = 20;
    jbConfig.maxDelayMs = 60;
    jitterBuffer_ = std::make_unique<AdaptiveJitterBuffer>(
        jbConfig,
        [this](uint32_t lastGoodId) {
            RequestIdr();
        }
    );

    // 5. Initialize Frame Reassembler
    reassembler_ = std::make_unique<FrameReassembler>(
        [this](VideoFrame frame) {
            if (decoder_) {
                decoder_->DecodeFrame(frame.data.data(), frame.data.size(), frame.timestampUs, frame.isKeyframe);
            }
            if (jitterBuffer_) {
                jitterBuffer_->PushFrame(frame);
            }
        },
        [this](uint32_t missingFrameId) {
            RequestIdr();
        }
    );

    // 6. Initialize UDP Server on port 5151
    udpServer_ = std::make_unique<IocpUdpServer>(5151);
    udpServer_->Start([this](const uint8_t* data, size_t size) {
        OnUdpPacketReceived(data, size);
    });

    // 7. Initialize TCP Control Client
    tcpClient_ = std::make_unique<TcpControlClient>();

    std::cout << "[ServiceManager] Initialized successfully" << std::endl;
    return true;
}

bool ServiceManager::ConnectDevice(const std::string& hostIp, uint16_t controlPort) {
    if (!tcpClient_) return false;
    bool ok = tcpClient_->Connect(hostIp, controlPort, [this](const std::string& msg) {
        OnTcpControlMessage(msg);
    });

    if (ok) {
        isStreaming_ = true;
        // Send startStream command
        std::string startStreamMsg = "{\"type\":\"startStream\",\"video\":{\"width\":1280,\"height\":720,\"fps\":30,\"bitrate\":2500000}}";
        tcpClient_->SendControlMessage(startStreamMsg);
    }
    return ok;
}

void ServiceManager::DisconnectDevice() {
    if (tcpClient_) {
        tcpClient_->SendControlMessage("{\"type\":\"stopStream\"}");
        tcpClient_->Disconnect();
    }
    isStreaming_ = false;
}

void ServiceManager::OnUdpPacketReceived(const uint8_t* data, size_t size) {
    if (reassembler_) {
        reassembler_->PushPacket(data, size);
        telemetry_.RecordBytesReceived(size);
    }
}

void ServiceManager::OnTcpControlMessage(const std::string& message) {
    std::cout << "[ServiceManager] Control message from mobile: " << message << std::endl;
}

void ServiceManager::SetZoom(float zoom) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage("{\"type\":\"setZoom\",\"zoom\":" + std::to_string(zoom) + "}");
    }
}

void ServiceManager::SetExposure(int32_t exposure) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage("{\"type\":\"setExposure\",\"exposure\":" + std::to_string(exposure) + "}");
    }
}

void ServiceManager::SetFocus(bool autoFocus, float distance) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage("{\"type\":\"setFocus\",\"autoFocus\":" + std::string(autoFocus ? "true" : "false") + ",\"distance\":" + std::to_string(distance) + "}");
    }
}

void ServiceManager::SetTorch(bool enabled) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage("{\"type\":\"setTorch\",\"enabled\":" + std::string(enabled ? "true" : "false") + "}");
    }
}

void ServiceManager::SwitchCamera(bool front) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage("{\"type\":\"switchCamera\",\"front\":" + std::string(front ? "true" : "false") + "}");
    }
}

void ServiceManager::RequestIdr() {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage("{\"type\":\"requestIdr\"}");
    }
}

uint64_t ServiceManager::GetSharedTextureHandle() const {
    return renderer_ ? renderer_->GetSharedTextureHandle() : 0;
}

std::string ServiceManager::GetTelemetryJson() {
    return telemetry_.ToJsonString(static_cast<uint8_t>(TransportType::WIFI_LAN), static_cast<uint8_t>(StreamProfileId::BALANCED));
}

void ServiceManager::Shutdown() {
    DisconnectDevice();
    if (udpServer_) {
        udpServer_->Stop();
        udpServer_.reset();
    }
    if (virtualCamera_) {
        virtualCamera_->Stop();
        virtualCamera_->UnregisterVirtualCamera();
        virtualCamera_.reset();
    }
    if (dshowFilter_) {
        dshowFilter_->Shutdown();
        dshowFilter_.reset();
    }
    if (decoder_) {
        decoder_->Shutdown();
        decoder_.reset();
    }
    if (renderer_) {
        renderer_->Shutdown();
        renderer_.reset();
    }
    std::cout << "[ServiceManager] Shutdown complete" << std::endl;
}

} // namespace veyra
