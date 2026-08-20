#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include "../../decoder/mf_decoder.h"

#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfvirtualcamera.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace veyra {

class MFVirtualCameraManager {
public:
    MFVirtualCameraManager();
    ~MFVirtualCameraManager();

    bool RegisterVirtualCamera(const std::wstring& friendlyName = L"Veyra Camera");
    bool UnregisterVirtualCamera();
    bool Start();
    void Stop();
    void PushFrame(const DecodedFrameNV12& frame);

    bool IsRegistered() const { return isRegistered_; }
    bool IsActive() const { return isActive_; }

private:
    std::wstring friendlyName_{L"Veyra Camera"};
    std::atomic<bool> isRegistered_{false};
    std::atomic<bool> isActive_{false};
    std::mutex mutex_;

#ifdef _WIN32
    ComPtr<IMFVirtualCamera> virtualCamera_;
#endif
};

} // namespace veyra
