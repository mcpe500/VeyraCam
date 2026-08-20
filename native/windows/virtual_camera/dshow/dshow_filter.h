#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include "../../decoder/mf_decoder.h"

#ifdef _WIN32
#include <windows.h>
#include <dshow.h>
#endif

namespace veyra {

// CLSID for Veyra DirectShow Source Filter: {7E88F932-B58A-4258-8CA2-3F9AEB191E04}
#ifdef _WIN32
static const GUID CLSID_VeyraDShowFilter = {
    0x7e88f932, 0xb58a, 0x4258, { 0x8c, 0xa2, 0x3f, 0x9a, 0xeb, 0x19, 0x1e, 0x04 }
};
#endif

class DShowFilter {
public:
    DShowFilter();
    ~DShowFilter();

    bool Initialize(uint32_t width = 1280, uint32_t height = 720, uint32_t fps = 30);
    void PushFrame(const DecodedFrameNV12& frame);
    void Shutdown();

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    bool IsActive() const { return isActive_; }

private:
    uint32_t width_{1280};
    uint32_t height_{720};
    uint32_t fps_{30};
    bool isActive_{false};
    std::mutex mutex_;
    std::vector<uint8_t> latestRgbFrame_;
};

} // namespace veyra
