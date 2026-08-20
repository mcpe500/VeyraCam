#include "virtual_camera_source.h"

namespace veyra {

VirtualCameraMediaStream::VirtualCameraMediaStream(uint32_t width, uint32_t height, uint32_t fps)
    : width_(width), height_(height), fps_(fps) {}

VirtualCameraMediaStream::~VirtualCameraMediaStream() {
    Stop();
}

void VirtualCameraMediaStream::Start() {
    isStreaming_ = true;
}

void VirtualCameraMediaStream::Stop() {
    isStreaming_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    while (!frameQueue_.empty()) {
        frameQueue_.pop();
    }
}

void VirtualCameraMediaStream::PushFrame(const DecodedFrameNV12& frame) {
    if (!isStreaming_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (frameQueue_.size() >= 3) {
        frameQueue_.pop(); // Drop oldest frame to maintain realtime latency
    }
    frameQueue_.push(frame);
}

} // namespace veyra
