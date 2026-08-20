#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace veyra {

struct DecodedFrameNV12 {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t stride{0};
    uint64_t timestampUs{0};
    std::vector<uint8_t> yPlane;
    std::vector<uint8_t> uvPlane;
#ifdef _WIN32
    ID3D11Texture2D* d3d11Texture{nullptr};
    HANDLE sharedHandle{nullptr};
#endif
};

class MFDecoder {
public:
    using FrameDecodedCallback = std::function<void(const DecodedFrameNV12& frame)>;

    MFDecoder();
    ~MFDecoder();

    bool Initialize(uint32_t width, uint32_t height, bool enableHardwareAcceleration = true);
    bool DecodeFrame(const uint8_t* h264Data, size_t dataSize, uint64_t timestampUs, bool isKeyframe);
    void Flush();
    void Shutdown();

    void SetFrameCallback(FrameDecodedCallback callback) { callback_ = callback; }

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    bool IsHardwareAccelerated() const { return isHardwareAccelerated_; }

private:
    uint32_t width_{1280};
    uint32_t height_{720};
    std::atomic<bool> initialized_{false};
    bool isHardwareAccelerated_{false};
    FrameDecodedCallback callback_;
    std::mutex mutex_;

#ifdef _WIN32
    ComPtr<IMFTransform> decoderMft_;
    ComPtr<IMFDXGIDeviceManager> dxgiDeviceManager_;
    ComPtr<ID3D11Device> d3d11Device_;
    ComPtr<ID3D11DeviceContext> d3d11Context_;
    DWORD inputStatusFlags_{0};
    DWORD outputStatusFlags_{0};
    MFT_OUTPUT_STREAM_INFO outputStreamInfo_{};
#endif
};

} // namespace veyra
