#include "service_manager.h"
#include <iostream>
#include <chrono>

namespace veyra {

namespace {

// Tiny JSON value extractor (avoids pulling in a full JSON dependency in core).
std::string ExtractJsonString(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    pos = json.find_first_not_of(" \t", pos + 1);
    if (pos == std::string::npos) return {};
    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return {};
        return json.substr(pos + 1, end - pos - 1);
    }
    size_t end = json.find_first_of(",}", pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos);
}

} // namespace

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

    // 6. Initialize UDP Server on port 5151 (media packets accepted only from
    //    the paired peer once SetExpectedPeer is applied; H-3).
    udpServer_ = std::make_unique<IocpUdpServer>(5151);
    udpServer_->Start([this](const uint8_t* data, size_t size) {
        OnUdpPacketReceived(data, size);
    });

    // 7. Initialize TCP Control Client
    tcpClient_ = std::make_unique<TcpControlClient>();

    std::cout << "[ServiceManager] Initialized successfully" << std::endl;
    return true;
}

bool ServiceManager::ConnectDevice(const std::string& hostIp, uint16_t controlPort,
                                   const std::string& pin, StatusCallback onStatus) {
    if (!tcpClient_) return false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pin_ = pin;
        token_.clear();
        crypto_.reset();
        hostIp_ = hostIp;
        pairingState_ = PairingState::AWAITING_CHALLENGE;
        statusCallback_ = std::move(onStatus);
    }

    bool ok = tcpClient_->Connect(hostIp, controlPort, [this](const std::string& msg) {
        OnTcpControlMessage(msg);
    });

    if (!ok) {
        std::lock_guard<std::mutex> lock(mutex_);
        pairingState_ = PairingState::DISCONNECTED;
        if (statusCallback_) statusCallback_("connect_failed");
        return false;
    }

    // The server responds with pairing_challenge on the control socket; the
    // pairing exchange completes asynchronously via OnTcpControlMessage.
    if (statusCallback_) statusCallback_("waiting_for_challenge");
    return true;
}

void ServiceManager::DisconnectDevice() {
    if (tcpClient_) {
        tcpClient_->Disconnect();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    isStreaming_ = false;
    pairingState_ = PairingState::DISCONNECTED;
    token_.clear();
    crypto_.reset();
    if (udpServer_) {
        udpServer_->SetExpectedPeer("");
    }
}

void ServiceManager::OnUdpPacketReceived(const uint8_t* data, size_t size) {
    if (reassembler_) {
        reassembler_->PushPacket(data, size);
        telemetry_.RecordBytesReceived(size);
    }
}

void ServiceManager::OnTcpControlMessage(const std::string& message) {
    std::cout << "[ServiceManager] Control message from mobile: " << message << std::endl;

    const std::string type = ExtractJsonString(message, "type");

    if (type == "pairing_challenge") {
        const std::string serverPubKey = ExtractJsonString(message, "server_pubkey");
        const std::string sessionIdStr = ExtractJsonString(message, "session_id");
        uint32_t sessionId = 0;
        try {
            sessionId = static_cast<uint32_t>(std::stoul(sessionIdStr));
        } catch (...) { sessionId = 0; }
        HandlePairingChallenge(serverPubKey, sessionId);
        return;
    }

    if (type == "pairing_ok") {
        const std::string token = ExtractJsonString(message, "token");
        HandlePairingResult(token, "");
        return;
    }

    if (type == "pairing_error") {
        const std::string reason = ExtractJsonString(message, "reason");
        HandlePairingResult("", reason);
        return;
    }

    // Other control messages (capabilities, etc.) are informational.
}

void ServiceManager::HandlePairingChallenge(const std::string& serverPubKeyB64, uint32_t sessionId) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pairingState_ != PairingState::AWAITING_CHALLENGE) {
            return;
        }
        if (serverPubKeyB64.empty()) {
            if (statusCallback_) statusCallback_("pairing_error:missing_pubkey");
            return;
        }
    }

    // C-2: derive mirrored session keys (client role = PC).
    auto serverPub = veyra::SessionCrypto::Base64Decode(serverPubKeyB64);
    if (serverPub.size() != veyra::VEYRA_PUBLIC_KEY_SIZE) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (statusCallback_) statusCallback_("pairing_error:invalid_pubkey");
        return;
    }

    const auto myKeys = veyra::SessionCrypto::GenerateKeyPair();
    auto crypto = std::make_shared<veyra::SessionCrypto>();
    bool ok = crypto->DeriveSessionKeys(
        /*serverRole=*/false,
        serverPub.data(),
        myKeys.publicKey.data(),
        myKeys.privateKey.data());
    if (!ok || !crypto->IsKeySet()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (statusCallback_) statusCallback_("pairing_error:derive_failed");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        crypto_ = crypto;
        if (reassembler_) {
            reassembler_->SetCrypto(crypto);
            if (sessionId != 0) {
                reassembler_->SetExpectedSessionId(sessionId);
            }
        }
        if (udpServer_) {
            udpServer_->SetExpectedPeer(hostIp_);
        }
        pairingState_ = PairingState::AWAITING_PAIRING_OK;
    }

    // Respond with the PIN + our ephemeral public key.
    const std::string clientPubB64 = veyra::SessionCrypto::Base64Encode(
        myKeys.publicKey.data(), myKeys.publicKey.size());
    std::string response = "{\"type\":\"pairing_response\",\"pin\":\"";
    response += pin_;
    response += "\",\"client_pubkey\":\"";
    response += clientPubB64;
    response += "\"}";
    if (tcpClient_) {
        tcpClient_->SendControlMessage(response);
    }
}

void ServiceManager::HandlePairingResult(const std::string& token, const std::string& errorReason) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!errorReason.empty()) {
        std::cerr << "[ServiceManager] Pairing failed: " << errorReason << std::endl;
        pairingState_ = PairingState::DISCONNECTED;
        isStreaming_ = false;
        crypto_.reset();
        const std::string status = "pairing_error:" + errorReason;
        if (statusCallback_) statusCallback_(status.c_str());
        return;
    }

    if (token.empty()) {
        if (statusCallback_) statusCallback_("pairing_error:missing_token");
        return;
    }

    token_ = token;
    pairingState_ = PairingState::PAIRED;
    isStreaming_ = true;
    std::cout << "[ServiceManager] Paired successfully; starting stream" << std::endl;
    if (statusCallback_) statusCallback_("paired");

    // Post-auth commands must carry the token.
    if (tcpClient_) {
        tcpClient_->SendControlMessage(
            BuildControlMessage("startStream", "{\"video\":{\"width\":1280,\"height\":720,\"fps\":30,\"bitrate\":2500000}}"));
        tcpClient_->SendControlMessage(BuildControlMessage("hello", ""));
    }
}

std::string ServiceManager::BuildControlMessage(const std::string& type, const std::string& payloadJson) const {
    std::string msg = "{\"type\":\"";
    msg += type;
    msg += "\"";
    if (!token_.empty()) {
        msg += ",\"auth_token\":\"";
        msg += token_;
        msg += "\"";
    }
    if (!payloadJson.empty()) {
        // Strip the payload's own braces and merge fields.
        std::string inner = payloadJson;
        if (!inner.empty() && inner.front() == '{') inner.erase(0, 1);
        if (!inner.empty() && inner.back() == '}') inner.pop_back();
        if (!inner.empty()) {
            msg += ",";
            msg += inner;
        }
    }
    msg += "}";
    return msg;
}

void ServiceManager::SetZoom(float zoom) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage(BuildControlMessage("setZoom", "{\"zoom\":" + std::to_string(zoom) + "}"));
    }
}

void ServiceManager::SetExposure(int32_t exposure) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage(BuildControlMessage("setExposure", "{\"exposure\":" + std::to_string(exposure) + "}"));
    }
}

void ServiceManager::SetFocus(bool autoFocus, float distance) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage(BuildControlMessage("setFocus", "{\"autoFocus\":" + std::string(autoFocus ? "true" : "false") + ",\"distance\":" + std::to_string(distance) + "}"));
    }
}

void ServiceManager::SetTorch(bool enabled) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage(BuildControlMessage("setTorch", "{\"enabled\":" + std::string(enabled ? "true" : "false") + "}"));
    }
}

void ServiceManager::SwitchCamera(bool front) {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage(BuildControlMessage("switchCamera", "{\"front\":" + std::string(front ? "true" : "false") + "}"));
    }
}

void ServiceManager::RequestIdr() {
    if (tcpClient_ && isStreaming_) {
        tcpClient_->SendControlMessage(BuildControlMessage("requestIdr", ""));
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
