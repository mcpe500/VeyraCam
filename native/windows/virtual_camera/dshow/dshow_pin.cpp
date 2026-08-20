#include "dshow_pin.h"
#include <cstring>

namespace veyra {

DShowPin::DShowPin(DShowFilter* filter) : filter_(filter) {}

DShowPin::~DShowPin() = default;

bool DShowPin::FillBuffer(uint8_t* buffer, size_t bufferSize) {
    if (!filter_ || !buffer) return false;
    memset(buffer, 0, bufferSize);
    return true;
}

} // namespace veyra
