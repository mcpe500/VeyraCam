#pragma once

#include "dshow_filter.h"

namespace veyra {

class DShowPin {
public:
    DShowPin(DShowFilter* filter);
    ~DShowPin();

    bool FillBuffer(uint8_t* buffer, size_t bufferSize);

private:
    DShowFilter* filter_;
};

} // namespace veyra
