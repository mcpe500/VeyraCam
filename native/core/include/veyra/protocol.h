#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace veyra {

constexpr uint32_t VEYRA_PROTOCOL_MAGIC = 0x56455952; // 'VEYR'
constexpr uint8_t  VEYRA_PROTOCOL_VERSION = 0x01;

constexpr size_t VEYRA_MAX_PACKET_SIZE = 1200;

// Packet Flags
enum PacketFlags : uint8_t {
    FLAG_NONE          = 0x00,
    FLAG_KEYFRAME      = 0x01, // Frame contains IDR / SPS / PPS
    FLAG_AUDIO         = 0x02, // Opus audio packet
    FLAG_ENCRYPTED     = 0x04, // Payload is encrypted with session key
    FLAG_TELEMETRY     = 0x08, // Telemetry packet
    FLAG_DISCONTINUITY = 0x10, // Indicates previous packet loss / transport switch
};

// Binary Control Command OpCodes (Hot Path)
enum class CommandOpCode : uint8_t {
    HELLO            = 0x01,
    CAPABILITIES     = 0x02,
    START_STREAM     = 0x03,
    STOP_STREAM      = 0x04,
    SET_ZOOM         = 0x05,
    SET_EXPOSURE     = 0x06,
    SET_FOCUS        = 0x07,
    REQUEST_IDR      = 0x08,
    SET_BITRATE      = 0x09,
    TRANSPORT_SWITCH = 0x0A,
    PING             = 0x0B,
    PONG             = 0x0C,
    STATS            = 0x0D,
};

// Stream Profile Identifiers
enum class StreamProfileId : uint8_t {
    ULTRA_LOW   = 0, // 320x240 @ 15fps, 200 kbps
    BLUETOOTH   = 1, // 640x360 @ 15fps, 400 kbps
    LOW         = 2, // 640x480 @ 24fps, 700 kbps
    BALANCED    = 3, // 1280x720 @ 30fps, 2.5 Mbps (Default)
    HIGH        = 4, // 1920x1080 @ 30fps, 4.5 Mbps
    PERFORMANCE = 5, // 1920x1080 @ 60fps, 8.0 Mbps
};

// Transport Type Identifiers
enum class TransportType : uint8_t {
    AUTO       = 0,
    USB        = 1,
    WIFI_LAN   = 2,
    WIFI_DIRECT= 3,
    BLUETOOTH  = 4,
};

#pragma pack(push, 1)

// Compact Binary Packet Header
struct PacketHeader {
    uint32_t magic;          // 0x56455952
    uint8_t  version;        // 0x01
    uint8_t  flags;          // Bitwise OR of PacketFlags
    uint16_t streamId;       // 0 for Video, 1 for Audio, 2 for Control
    uint32_t sessionId;      // Unique session identifier
    uint32_t frameId;        // Monotonic frame counter
    uint32_t sequence;       // Monotonic packet sequence counter
    uint16_t fragmentIndex;  // 0-indexed fragment index
    uint16_t fragmentCount;  // Total fragments in this frame
    uint64_t timestampUs;    // Frame capture timestamp in microseconds
    uint16_t payloadLength;  // Payload byte count (0 to 1168)
};

// Binary Control Command Header
struct ControlCommandHeader {
    uint8_t  opCode;         // CommandOpCode
    uint8_t  reserved;       // 0
    uint16_t payloadLength;  // Body length
    uint32_t sequence;       // Command sequence ID
};

// 0x05 SET_ZOOM
struct SetZoomCommand {
    float zoomFactor;        // 1.0f to 10.0f
};

// 0x06 SET_EXPOSURE
struct SetExposureCommand {
    int32_t exposureCompensation; // EV step
};

// 0x07 SET_FOCUS
struct SetFocusCommand {
    uint8_t autoFocus;       // 1 = Auto, 0 = Manual
    float   focusDistance;   // 0.0f (infinity) to 1.0f (closest macro)
};

// 0x08 REQUEST_IDR
struct RequestIdrCommand {
    uint32_t lastGoodFrameId;
};

// 0x09 SET_BITRATE
struct SetBitrateCommand {
    uint32_t targetBitrateBps;
};

// 0x0A TRANSPORT_SWITCH
struct TransportSwitchCommand {
    uint8_t targetTransport; // TransportType
    uint32_t candidateSessionId;
};

// 0x0B PING / 0x0C PONG
struct PingPongMessage {
    uint64_t clientTimestampUs;
    uint64_t serverTimestampUs;
};

// 0x0D STATS
struct TelemetryStatsPayload {
    float    fps;
    uint32_t bitrateBps;
    float    latencyMs;
    float    packetLossPercent;
    uint8_t  batteryPercent;
    int8_t   temperatureCelsius;
    uint8_t  currentTransport;
    uint8_t  activeProfile;
};

#pragma pack(pop)

constexpr size_t VEYRA_HEADER_SIZE = sizeof(PacketHeader);
constexpr size_t VEYRA_MAX_PAYLOAD_SIZE = VEYRA_MAX_PACKET_SIZE - VEYRA_HEADER_SIZE;

// Helper serialization functions
bool SerializeHeader(const PacketHeader& header, uint8_t* outBuffer, size_t bufferSize);
bool DeserializeHeader(const uint8_t* inBuffer, size_t bufferSize, PacketHeader& outHeader);

} // namespace veyra
