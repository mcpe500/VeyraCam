#include "veyra/telemetry.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace veyra {

TelemetryCollector::TelemetryCollector()
    : totalFramesCounted_(0),
      lostPacketsCount_(0),
      totalPacketsCount_(0),
      temperatureC_(35),
      batteryPct_(100) {}

void TelemetryCollector::RecordFrameTiming(uint32_t /*frameId*/, const StageTiming& timing) {
    std::lock_guard<std::mutex> lock(mutex_);
    timingHistory_.push_back(timing);
    if (timingHistory_.size() > 120) { // Keep ~2-4 seconds of history
        timingHistory_.pop_front();
    }
    totalFramesCounted_++;
}

void TelemetryCollector::RecordBytesReceived(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    bytesHistory_.push_back({nowUs, bytes});

    // Remove entries older than 1 second
    while (!bytesHistory_.empty() && (nowUs - bytesHistory_.front().first) > 1000000) {
        bytesHistory_.pop_front();
    }
}

void TelemetryCollector::RecordPacketLoss(uint32_t lostCount, uint32_t totalExpected) {
    std::lock_guard<std::mutex> lock(mutex_);
    lostPacketsCount_ += lostCount;
    totalPacketsCount_ += totalExpected;
}

void TelemetryCollector::SetDeviceThermal(int8_t celsius, uint8_t batteryPercent) {
    std::lock_guard<std::mutex> lock(mutex_);
    temperatureC_ = celsius;
    batteryPct_ = batteryPercent;
}

LatencyBreakdown TelemetryCollector::ComputeLatencyBreakdownLocked() const {
    LatencyBreakdown bd{};
    if (timingHistory_.empty()) {
        return bd;
    }

    double capToEnc = 0, encDur = 0, net = 0, jitter = 0, dec = 0, pres = 0, total = 0;
    size_t validSamples = 0;

    for (const auto& t : timingHistory_) {
        if (t.presentUs > t.captureUs && t.captureUs > 0) {
            double c2e = (t.encodeUs >= t.captureUs) ? (t.encodeUs - t.captureUs) / 1000.0 : 0;
            double n = (t.netRecvUs >= t.netSendUs) ? (t.netRecvUs - t.netSendUs) / 1000.0 : 0;
            double j = (t.jitterReadyUs >= t.netRecvUs) ? (t.jitterReadyUs - t.netRecvUs) / 1000.0 : 0;
            double d = (t.decodeUs >= t.jitterReadyUs) ? (t.decodeUs - t.jitterReadyUs) / 1000.0 : 0;
            double p = (t.presentUs >= t.decodeUs) ? (t.presentUs - t.decodeUs) / 1000.0 : 0;
            double tot = (t.presentUs - t.captureUs) / 1000.0;

            capToEnc += c2e;
            net += n;
            jitter += j;
            dec += d;
            pres += p;
            total += tot;
            validSamples++;
        }
    }

    if (validSamples > 0) {
        bd.captureToEncodeMs = static_cast<float>(capToEnc / validSamples);
        bd.networkTransitMs = static_cast<float>(net / validSamples);
        bd.jitterBufferDelayMs = static_cast<float>(jitter / validSamples);
        bd.decodeDurationMs = static_cast<float>(dec / validSamples);
        bd.presentationDelayMs = static_cast<float>(pres / validSamples);
        bd.endToEndLatencyMs = static_cast<float>(total / validSamples);
    }
    return bd;
}

LatencyBreakdown TelemetryCollector::GetAverageLatencyBreakdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ComputeLatencyBreakdownLocked();
}

TelemetryStatsPayload TelemetryCollector::GetCurrentStatsPayload(uint8_t activeTransport, uint8_t activeProfile) const {
    std::lock_guard<std::mutex> lock(mutex_);
    TelemetryStatsPayload stats{};

    // Calculate rolling FPS from timingHistory count in last second
    stats.fps = static_cast<float>(std::min(timingHistory_.size(), size_t(30)));

    // Calculate Bitrate Bps from bytes in last second
    size_t bytesSum = 0;
    for (const auto& kv : bytesHistory_) {
        bytesSum += kv.second;
    }
    stats.bitrateBps = static_cast<uint32_t>(bytesSum * 8);

    // Calculate packet loss
    if (totalPacketsCount_ > 0) {
        stats.packetLossPercent = (static_cast<float>(lostPacketsCount_) / totalPacketsCount_) * 100.0f;
    }

    auto bd = ComputeLatencyBreakdownLocked();
    stats.latencyMs = bd.endToEndLatencyMs;
    stats.batteryPercent = batteryPct_;
    stats.temperatureCelsius = temperatureC_;
    stats.currentTransport = activeTransport;
    stats.activeProfile = activeProfile;

    return stats;
}

std::string TelemetryCollector::ToJsonString(uint8_t activeTransport, uint8_t activeProfile) const {
    auto stats = GetCurrentStatsPayload(activeTransport, activeProfile);
    auto bd = GetAverageLatencyBreakdown();

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "{\n"
       << "  \"fps\": " << stats.fps << ",\n"
       << "  \"bitrate_bps\": " << stats.bitrateBps << ",\n"
       << "  \"latency_ms\": " << stats.latencyMs << ",\n"
       << "  \"packet_loss_pct\": " << stats.packetLossPercent << ",\n"
       << "  \"battery_pct\": " << static_cast<int>(stats.batteryPercent) << ",\n"
       << "  \"temperature_c\": " << static_cast<int>(stats.temperatureCelsius) << ",\n"
       << "  \"breakdown\": {\n"
       << "    \"capture_to_encode_ms\": " << bd.captureToEncodeMs << ",\n"
       << "    \"network_transit_ms\": " << bd.networkTransitMs << ",\n"
       << "    \"jitter_delay_ms\": " << bd.jitterBufferDelayMs << ",\n"
       << "    \"decode_ms\": " << bd.decodeDurationMs << ",\n"
       << "    \"present_ms\": " << bd.presentationDelayMs << "\n"
       << "  }\n"
       << "}";
    return ss.str();
}

void TelemetryCollector::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    timingHistory_.clear();
    bytesHistory_.clear();
    lostPacketsCount_ = 0;
    totalPacketsCount_ = 0;
}

} // namespace veyra
