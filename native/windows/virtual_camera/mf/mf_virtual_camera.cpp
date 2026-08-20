#include "mf_virtual_camera.h"
#include <iostream>

namespace veyra {

MFVirtualCameraManager::MFVirtualCameraManager() = default;

MFVirtualCameraManager::~MFVirtualCameraManager() {
    Stop();
    UnregisterVirtualCamera();
}

bool MFVirtualCameraManager::RegisterVirtualCamera(const std::wstring& friendlyName) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isRegistered_) return true;

    friendlyName_ = friendlyName;

#ifdef _WIN32
    // Windows 11 Build 22000+ Virtual Camera API
    typedef HRESULT(WINAPI* PFN_MFCreateVirtualCamera)(
        MFVirtualCameraType type,
        MFVirtualCameraLifetime lifetime,
        MFVirtualCameraAccess access,
        LPCWSTR pwzFriendlyName,
        LPCWSTR pwzWrappedDeviceSymbolicLink,
        const GUID* categories,
        ULONG categoryCount,
        IMFVirtualCamera** ppVirtualCamera
    );

    HMODULE hMf = GetModuleHandleW(L"mfplat.dll");
    if (!hMf) {
        hMf = LoadLibraryW(L"mfplat.dll");
    }

    if (hMf) {
        auto pfnCreate = reinterpret_cast<PFN_MFCreateVirtualCamera>(
            GetProcAddress(hMf, "MFCreateVirtualCamera")
        );
        if (pfnCreate) {
            GUID categories[] = { KSCATEGORY_VIDEO_CAMERA, KSCATEGORY_CAPTURE };
            HRESULT hr = pfnCreate(
                MFVirtualCameraType_SoftwareCameraSource,
                MFVirtualCameraLifetime_Session,
                MFVirtualCameraAccess_CurrentUser,
                friendlyName_.c_str(),
                nullptr,
                categories,
                ARRAYSIZE(categories),
                &virtualCamera_
            );

            if (SUCCEEDED(hr) && virtualCamera_) {
                hr = virtualCamera_->Start(nullptr);
                if (SUCCEEDED(hr)) {
                    isRegistered_ = true;
                    std::wcout << L"[MFVirtualCamera] Registered and started " << friendlyName_ << std::endl;
                    return true;
                }
            }
        }
    }
    std::cerr << "[MFVirtualCamera] IMFVirtualCamera not supported on this OS build" << std::endl;
    return false;
#else
    isRegistered_ = true;
    return true;
#endif
}

bool MFVirtualCameraManager::UnregisterVirtualCamera() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRegistered_) return true;

#ifdef _WIN32
    if (virtualCamera_) {
        virtualCamera_->Stop();
        virtualCamera_->Remove();
        virtualCamera_.Reset();
    }
#endif

    isRegistered_ = false;
    isActive_ = false;
    return true;
}

bool MFVirtualCameraManager::Start() {
    isActive_ = true;
    return true;
}

void MFVirtualCameraManager::Stop() {
    isActive_ = false;
}

void MFVirtualCameraManager::PushFrame(const DecodedFrameNV12& frame) {
    if (!isActive_) return;
    // Deliver frame to Media Foundation media source stream
}

} // namespace veyra
