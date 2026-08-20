#include "veyra/protocol.h"
#include <cstring>

namespace veyra {

bool SerializeHeader(const PacketHeader& header, uint8_t* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize < sizeof(PacketHeader)) {
        return false;
    }
    std::memcpy(outBuffer, &header, sizeof(PacketHeader));
    return true;
}

bool DeserializeHeader(const uint8_t* inBuffer, size_t bufferSize, PacketHeader& outHeader) {
    if (!inBuffer || bufferSize < sizeof(PacketHeader)) {
        return false;
    }
    std::memcpy(&outHeader, inBuffer, sizeof(PacketHeader));
    if (outHeader.magic != VEYRA_PROTOCOL_MAGIC || outHeader.version != VEYRA_PROTOCOL_VERSION) {
        return false;
    }
    return true;
}

} // namespace veyra
