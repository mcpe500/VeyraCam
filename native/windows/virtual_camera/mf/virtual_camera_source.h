#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>
#include "../../decoder/mf_decoder.h"

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace veyra {

class VirtualCameraMediaStream {
public:
    VirtualCameraMediaStream(uint32_t width = 1280, uint32_t height = 720, uint32_t fps = 30);
    ~VirtualCameraMediaStream();

    void PushFrame(const DecodedFrameNV12& frame);
    void Start();
    void Stop();

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    uint32_t GetFps() const { return fps_; }

private:
    uint32_t width_{1280};
    uint32_t height_{720};
    uint32_t fps_{30};
    std::atomic<bool> isStreaming_{false};
    std::mutex mutex_;
    std::queue<DecodedFrameNV12> frameQueue_;
};

} // namespace veyra
