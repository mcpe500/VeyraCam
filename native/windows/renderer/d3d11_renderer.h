#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include "../decoder/mf_decoder.h"

#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace veyra {

class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    bool Initialize(uint32_t width, uint32_t height);
    bool RenderFrame(const DecodedFrameNV12& frame);
    void Shutdown();

    uint64_t GetSharedTextureHandle() const;
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

private:
    uint32_t width_{1280};
    uint32_t height_{720};
    bool initialized_{false};
    std::mutex mutex_;
    uint64_t sharedHandle_{0};

#ifdef _WIN32
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11Texture2D> sharedTexture_;
    ComPtr<IDXGIResource> dxgiResource_;
#endif
};

} // namespace veyra
