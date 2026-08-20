#include "veyra/session.h"
#include <random>
#include <cstring>

namespace veyra {

VeyraSession::VeyraSession() {
    std::random_device rd;
    sessionId_ = rd();

    transportManager_ = std::make_unique<AutoTransportManager>();
    packetizer_ = std::make_unique<Packetizer>(sessionId_);
    crypto_ = std::make_unique<SessionCrypto>();

    reassembler_ = std::make_unique<FrameReassembler>(
        [this](VideoFrame frame) {
            this->OnAssembledFrameReady(std::move(frame));
        },
        [this](uint32_t /*frameId*/) {
            this->RequestIdr();
        }
    );

    jitterBuffer_ = std::make_unique<AdaptiveJitterBuffer>(
        JitterBufferConfig{20, 80, 10},
        [this](uint32_t /*lastGoodFrameId*/) {
            this->RequestIdr();
        }
    );
}

VeyraSession::~VeyraSession() {
    StopSession();
}

bool VeyraSession::StartSession(const StreamConfig& config) {
    config_ = config;
    SetState(SessionState::CONNECTING, "Connecting transports...");

    transportManager_->SetPreferredTransportMode(config.preferredTransport);

    if (config.enableEncryption) {
        // Ephemeral crypto key generation if requested
        auto keyPair = SessionCrypto::GenerateKeyPair();
        crypto_->SetSessionKey(keyPair.publicKey.data());
    }

    SetState(SessionState::STREAMING, "Session active and streaming");
    return true;
}

void VeyraSession::StopSession() {
    if (state_ == SessionState::IDLE || state_ == SessionState::DISCONNECTED) {
        return;
    }
    SetState(SessionState::DISCONNECTED, "Session stopped");
    if (jitterBuffer_) {
        jitterBuffer_->Flush();
    }
    if (reassembler_) {
        reassembler_->Reset();
    }
    telemetry_.Reset();
}

void VeyraSession::PauseSession() {
    if (state_ == SessionState::STREAMING) {
        SetState(SessionState::PAUSED, "Session paused");
    }
}

void VeyraSession::ResumeSession() {
    if (state_ == SessionState::PAUSED) {
        SetState(SessionState::STREAMING, "Session resumed");
        RequestIdr();
    }
}

void VeyraSession::SetZoom(float factor) {
    cameraState_.zoom = factor;
    // Dispatch binary command 0x05
    ControlCommandHeader cmd{};
    cmd.opCode = static_cast<uint8_t>(CommandOpCode::SET_ZOOM);
    cmd.payloadLength = sizeof(SetZoomCommand);
    SetZoomCommand payload{factor};

    std::vector<uint8_t> buffer(sizeof(ControlCommandHeader) + sizeof(SetZoomCommand));
    std::memcpy(buffer.data(), &cmd, sizeof(cmd));
    std::memcpy(buffer.data() + sizeof(cmd), &payload, sizeof(payload));
    transportManager_->SendPacket(buffer.data(), buffer.size());
}

void VeyraSession::SetExposure(int32_t step) {
    cameraState_.exposure = step;
    ControlCommandHeader cmd{};
    cmd.opCode = static_cast<uint8_t>(CommandOpCode::SET_EXPOSURE);
    cmd.payloadLength = sizeof(SetExposureCommand);
    SetExposureCommand payload{step};

    std::vector<uint8_t> buffer(sizeof(ControlCommandHeader) + sizeof(SetExposureCommand));
    std::memcpy(buffer.data(), &cmd, sizeof(cmd));
    std::memcpy(buffer.data() + sizeof(cmd), &payload, sizeof(payload));
    transportManager_->SendPacket(buffer.data(), buffer.size());
}

void VeyraSession::SetFocus(bool autoFocus, float distance) {
    cameraState_.autoFocus = autoFocus;
    cameraState_.manualFocusDistance = distance;
    ControlCommandHeader cmd{};
    cmd.opCode = static_cast<uint8_t>(CommandOpCode::SET_FOCUS);
    cmd.payloadLength = sizeof(SetFocusCommand);
    SetFocusCommand payload{static_cast<uint8_t>(autoFocus ? 1 : 0), distance};

    std::vector<uint8_t> buffer(sizeof(ControlCommandHeader) + sizeof(SetFocusCommand));
    std::memcpy(buffer.data(), &cmd, sizeof(cmd));
    std::memcpy(buffer.data() + sizeof(cmd), &payload, sizeof(payload));
    transportManager_->SendPacket(buffer.data(), buffer.size());
}

void VeyraSession::SetTorch(bool enable) {
    cameraState_.torchOn = enable;
}

void VeyraSession::SwitchCamera(bool front) {
    cameraState_.useFrontCamera = front;
    RequestIdr();
}

void VeyraSession::RequestIdr() {
    ControlCommandHeader cmd{};
    cmd.opCode = static_cast<uint8_t>(CommandOpCode::REQUEST_IDR);
    cmd.payloadLength = sizeof(RequestIdrCommand);
    RequestIdrCommand payload{0};

    std::vector<uint8_t> buffer(sizeof(ControlCommandHeader) + sizeof(RequestIdrCommand));
    std::memcpy(buffer.data(), &cmd, sizeof(cmd));
    std::memcpy(buffer.data() + sizeof(cmd), &payload, sizeof(payload));
    transportManager_->SendPacket(buffer.data(), buffer.size());
}

void VeyraSession::UpdateBitrate(uint32_t bitrateBps) {
    config_.bitrateBps = bitrateBps;
    ControlCommandHeader cmd{};
    cmd.opCode = static_cast<uint8_t>(CommandOpCode::SET_BITRATE);
    cmd.payloadLength = sizeof(SetBitrateCommand);
    SetBitrateCommand payload{bitrateBps};

    std::vector<uint8_t> buffer(sizeof(ControlCommandHeader) + sizeof(SetBitrateCommand));
    std::memcpy(buffer.data(), &cmd, sizeof(cmd));
    std::memcpy(buffer.data() + sizeof(cmd), &payload, sizeof(payload));
    transportManager_->SendPacket(buffer.data(), buffer.size());
}

void VeyraSession::SetPreferredTransport(TransportType type) {
    config_.preferredTransport = type;
    transportManager_->SetPreferredTransportMode(type);
}

void VeyraSession::RegisterTransport(std::shared_ptr<Transport> transport) {
    if (!transport) return;
    transport->SetPacketReceivedCallback([this](const uint8_t* data, size_t size) {
        this->telemetry_.RecordBytesReceived(size);
        this->reassembler_->PushPacket(data, size);
    });
    transportManager_->RegisterTransport(transport);
}

void VeyraSession::SetState(SessionState newState, const std::string& msg) {
    state_ = newState;
    if (stateCallback_) {
        stateCallback_(newState, msg);
    }
}

void VeyraSession::OnAssembledFrameReady(VideoFrame frame) {
    if (jitterBuffer_) {
        jitterBuffer_->PushFrame(std::move(frame));
        auto readyFrame = jitterBuffer_->PopNextFrame();
        if (readyFrame.has_value()) {
            if (frameCallback_) {
                frameCallback_(readyFrame.value());
            }
        }
    }
}

} // namespace veyra
