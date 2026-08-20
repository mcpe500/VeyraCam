#include <jni.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <android/log.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "veyra/protocol.h"
#include "veyra/packetizer.h"
#include "veyra/session.h"
#include "veyra/telemetry.h"

#define TAG "VeyraJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace {

class NativeSenderContext {
public:
    NativeSenderContext(uint32_t sessionId)
        : sessionId_(sessionId),
          packetizer_(std::make_unique<veyra::Packetizer>(sessionId)),
          udpSocket_(-1),
          udpPort_(0) {
        memset(&udpAddr_, 0, sizeof(udpAddr_));
    }

    ~NativeSenderContext() {
        CloseUdp();
    }

    bool ConfigureUdp(const std::string& host, int port) {
        std::lock_guard<std::mutex> lock(mutex_);
        CloseUdp();

        udpSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket_ < 0) {
            LOGE("Failed to create UDP socket");
            return false;
        }

        // Set non-blocking & buffer size
        int flags = fcntl(udpSocket_, F_GETFL, 0);
        fcntl(udpSocket_, F_SETFL, flags | O_NONBLOCK);

        int sendBuf = 1024 * 1024; // 1 MB send buffer
        setsockopt(udpSocket_, SOL_SOCKET, SO_SNDBUF, &sendBuf, sizeof(sendBuf));

        udpAddr_.sin_family = AF_INET;
        udpAddr_.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &udpAddr_.sin_addr) <= 0) {
            LOGE("Invalid UDP IP address: %s", host.c_str());
            close(udpSocket_);
            udpSocket_ = -1;
            return false;
        }

        udpHost_ = host;
        udpPort_ = port;
        LOGI("Configured native UDP destination to %s:%d", host.c_str(), port);
        return true;
    }

    void SendVideoNal(const uint8_t* nalData, size_t nalLength, bool isKeyframe, uint64_t timestampUs) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (nalLength == 0 || !packetizer_) return;

        frameCounter_++;
        auto packets = packetizer_->PacketizeFrame(
            nalData, nalLength, frameCounter_, timestampUs, isKeyframe
        );

        veyra::StageTiming timing;
        timing.captureUs = timestampUs;
        timing.encodeUs = timestampUs + 3000;
        timing.netSendUs = timestampUs + 4000;
        telemetry_.RecordFrameTiming(frameCounter_, timing);

        if (udpSocket_ >= 0) {
            for (const auto& pkt : packets) {
                sendto(udpSocket_, pkt.data(), pkt.size(), 0,
                       reinterpret_cast<struct sockaddr*>(&udpAddr_), sizeof(udpAddr_));
                telemetry_.RecordBytesReceived(pkt.size());
            }
        }
    }

    void SendAudioFrame(const uint8_t* audioData, size_t audioLength, uint64_t timestampUs) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (audioLength == 0 || !packetizer_) return;

        audioCounter_++;
        auto pkt = packetizer_->PacketizeAudio(audioData, audioLength, audioCounter_, timestampUs);

        if (udpSocket_ >= 0) {
            sendto(udpSocket_, pkt.data(), pkt.size(), 0,
                   reinterpret_cast<struct sockaddr*>(&udpAddr_), sizeof(udpAddr_));
        }
    }

    void CloseUdp() {
        if (udpSocket_ >= 0) {
            close(udpSocket_);
            udpSocket_ = -1;
        }
    }

    std::string GetTelemetryJson() {
        std::lock_guard<std::mutex> lock(mutex_);
        return telemetry_.ToJsonString(static_cast<uint8_t>(veyra::TransportType::WIFI_LAN), static_cast<uint8_t>(veyra::StreamProfileId::BALANCED));
    }

    uint32_t GetSessionId() const { return sessionId_; }

private:
    uint32_t sessionId_;
    std::unique_ptr<veyra::Packetizer> packetizer_;
    veyra::TelemetryCollector telemetry_;
    uint32_t frameCounter_{0};
    uint32_t audioCounter_{0};

    int udpSocket_{-1};
    std::string udpHost_;
    int udpPort_{0};
    struct sockaddr_in udpAddr_;
    std::mutex mutex_;
};

} // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_veyra_cam_service_VeyraNativeBridge_nativeCreateSession(
    JNIEnv* env, jobject thiz, jint sessionId) {
    auto* ctx = new NativeSenderContext(static_cast<uint32_t>(sessionId));
    LOGI("Created NativeSenderContext for sessionId: %u", sessionId);
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT void JNICALL
Java_com_veyra_cam_service_VeyraNativeBridge_nativeDestroySession(
    JNIEnv* env, jobject thiz, jlong handle) {
    if (handle != 0) {
        auto* ctx = reinterpret_cast<NativeSenderContext*>(handle);
        LOGI("Destroying NativeSenderContext for sessionId: %u", ctx->GetSessionId());
        delete ctx;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_veyra_cam_service_VeyraNativeBridge_nativeConfigureUdpDestination(
    JNIEnv* env, jobject thiz, jlong handle, jstring host, jint port) {
    if (handle == 0) return JNI_FALSE;
    auto* ctx = reinterpret_cast<NativeSenderContext*>(handle);

    const char* hostStr = env->GetStringUTFChars(host, nullptr);
    std::string hostCpp(hostStr);
    env->ReleaseStringUTFChars(host, hostStr);

    bool ok = ctx->ConfigureUdp(hostCpp, static_cast<int>(port));
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_veyra_cam_service_VeyraNativeBridge_nativeSendVideoNal(
    JNIEnv* env, jobject thiz, jlong handle, jobject byteBuffer, jint offset, jint length, jboolean isKeyframe, jlong timestampUs) {
    if (handle == 0 || byteBuffer == nullptr || length <= 0) return;
    auto* ctx = reinterpret_cast<NativeSenderContext*>(handle);

    auto* bufferPtr = static_cast<uint8_t*>(env->GetDirectBufferAddress(byteBuffer));
    if (!bufferPtr) return;

    ctx->SendVideoNal(
        bufferPtr + offset,
        static_cast<size_t>(length),
        isKeyframe == JNI_TRUE,
        static_cast<uint64_t>(timestampUs)
    );
}

JNIEXPORT void JNICALL
Java_com_veyra_cam_service_VeyraNativeBridge_nativeSendAudioFrame(
    JNIEnv* env, jobject thiz, jlong handle, jobject byteBuffer, jint offset, jint length, jlong timestampUs) {
    if (handle == 0 || byteBuffer == nullptr || length <= 0) return;
    auto* ctx = reinterpret_cast<NativeSenderContext*>(handle);

    auto* bufferPtr = static_cast<uint8_t*>(env->GetDirectBufferAddress(byteBuffer));
    if (!bufferPtr) return;

    ctx->SendAudioFrame(
        bufferPtr + offset,
        static_cast<size_t>(length),
        static_cast<uint64_t>(timestampUs)
    );
}

JNIEXPORT jstring JNICALL
Java_com_veyra_cam_service_VeyraNativeBridge_nativeGetTelemetryJson(
    JNIEnv* env, jobject thiz, jlong handle) {
    if (handle == 0) return env->NewStringUTF("{}");
    auto* ctx = reinterpret_cast<NativeSenderContext*>(handle);
    std::string json = ctx->GetTelemetryJson();
    return env->NewStringUTF(json.c_str());
}

} // extern "C"
