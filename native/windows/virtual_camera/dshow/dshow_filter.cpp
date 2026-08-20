#include "dshow_filter.h"
#include <iostream>

namespace veyra {

DShowFilter::DShowFilter() = default;

DShowFilter::~DShowFilter() {
    Shutdown();
}

bool DShowFilter::Initialize(uint32_t width, uint32_t height, uint32_t fps) {
    std::lock_guard<std::mutex> lock(mutex_);
    width_ = width;
    height_ = height;
    fps_ = fps;
    latestRgbFrame_.resize(width_ * height_ * 3, 0);
    isActive_ = true;
    return true;
}

void DShowFilter::PushFrame(const DecodedFrameNV12& frame) {
    if (!isActive_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    // Store latest frame for DirectShow sample delivery
}

void DShowFilter::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    isActive_ = false;
    latestRgbFrame_.clear();
}

} // namespace veyra
