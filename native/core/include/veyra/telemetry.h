#pragma once

#include "protocol.h"
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace veyra {

struct StageTiming {
    uint64_t captureUs{0};
    uint64_t encodeUs{0};
    uint64_t netSendUs{0};
    uint64_t netRecvUs{0};
    uint64_t jitterReadyUs{0};
    uint64_t decodeUs{0};
    uint64_t presentUs{0};
};

struct LatencyBreakdown {
    float captureToEncodeMs{0.0f};
    float encodeDurationMs{0.0f};
    float networkTransitMs{0.0f};
    float jitterBufferDelayMs{0.0f};
    float decodeDurationMs{0.0f};
    float presentationDelayMs{0.0f};
    float endToEndLatencyMs{0.0f};
};

class TelemetryCollector {
public:
    TelemetryCollector();
    ~TelemetryCollector() = default;

    void RecordFrameTiming(uint32_t frameId, const StageTiming& timing);
    void RecordBytesReceived(size_t bytes);
    void RecordPacketLoss(uint32_t lostCount, uint32_t totalExpected);
    void SetDeviceThermal(int8_t celsius, uint8_t batteryPercent);

    // Compute averaged metrics over rolling 1-second window
    LatencyBreakdown GetAverageLatencyBreakdown() const;
    TelemetryStatsPayload GetCurrentStatsPayload(uint8_t activeTransport, uint8_t activeProfile) const;
    std::string ToJsonString(uint8_t activeTransport, uint8_t activeProfile) const;

    void Reset();

private:
    mutable std::mutex mutex_;
    std::deque<StageTiming> timingHistory_;
    std::deque<std::pair<uint64_t, size_t>> bytesHistory_;
    
    uint64_t totalFramesCounted_{0};
    uint64_t lostPacketsCount_{0};
    uint64_t totalPacketsCount_{0};
    int8_t temperatureC_{35};
    uint8_t batteryPct_{100};
};

} // namespace veyra
