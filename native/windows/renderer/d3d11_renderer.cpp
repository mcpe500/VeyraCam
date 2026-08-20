#include "d3d11_renderer.h"
#include <iostream>

namespace veyra {

D3D11Renderer::D3D11Renderer() = default;

D3D11Renderer::~D3D11Renderer() {
    Shutdown();
}

bool D3D11Renderer::Initialize(uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    width_ = width;
    height_ = height;

#ifdef _WIN32
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &device_,
        &featureLevel,
        &context_
    );

    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] Failed to create D3D11 device" << std::endl;
        return false;
    }

    // Create Shared NV12 Texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width_;
    desc.Height = height_;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    hr = device_->CreateTexture2D(&desc, nullptr, &sharedTexture_);
    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] Failed to create shared NV12 texture" << std::endl;
        return false;
    }

    hr = sharedTexture_.As(&dxgiResource_);
    if (SUCCEEDED(hr)) {
        HANDLE h = nullptr;
        dxgiResource_->GetSharedHandle(&h);
        sharedHandle_ = reinterpret_cast<uint64_t>(h);
    }
#else
    sharedHandle_ = 0x12345678;
#endif

    initialized_ = true;
    return true;
}

bool D3D11Renderer::RenderFrame(const DecodedFrameNV12& frame) {
    if (!initialized_) return false;
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef _WIN32
    if (!context_ || !sharedTexture_) return false;

    if (!frame.yPlane.empty() && !frame.uvPlane.empty()) {
        std::vector<uint8_t> contiguousNV12;
        contiguousNV12.reserve(frame.yPlane.size() + frame.uvPlane.size());
        contiguousNV12.insert(contiguousNV12.end(), frame.yPlane.begin(), frame.yPlane.end());
        contiguousNV12.insert(contiguousNV12.end(), frame.uvPlane.begin(), frame.uvPlane.end());

        context_->UpdateSubresource(
            sharedTexture_.Get(),
            0,
            nullptr,
            contiguousNV12.data(),
            width_,
            0
        );
    }
#endif

    return true;
}

uint64_t D3D11Renderer::GetSharedTextureHandle() const {
    return sharedHandle_;
}

void D3D11Renderer::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) return;

#ifdef _WIN32
    dxgiResource_.Reset();
    sharedTexture_.Reset();
    context_.Reset();
    device_.Reset();
#endif

    sharedHandle_ = 0;
    initialized_ = false;
}

} // namespace veyra
