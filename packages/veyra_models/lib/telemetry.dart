import 'package:meta/meta.dart';

@immutable
class LatencyBreakdown {
  final double captureToEncodeMs;
  final double encodeDurationMs;
  final double networkTransitMs;
  final double jitterBufferDelayMs;
  final double decodeDurationMs;
  final double presentationDelayMs;
  final double endToEndLatencyMs;

  const LatencyBreakdown({
    this.captureToEncodeMs = 0.0,
    this.encodeDurationMs = 0.0,
    this.networkTransitMs = 0.0,
    this.jitterBufferDelayMs = 0.0,
    this.decodeDurationMs = 0.0,
    this.presentationDelayMs = 0.0,
    this.endToEndLatencyMs = 0.0,
  });

  factory LatencyBreakdown.fromJson(Map<String, dynamic> json) {
    return LatencyBreakdown(
      captureToEncodeMs: (json['capture_to_encode_ms'] as num?)?.toDouble() ?? 0.0,
      encodeDurationMs: (json['encode_duration_ms'] as num?)?.toDouble() ?? 0.0,
      networkTransitMs: (json['network_transit_ms'] as num?)?.toDouble() ?? 0.0,
      jitterBufferDelayMs: (json['jitter_buffer_delay_ms'] as num?)?.toDouble() ?? 0.0,
      decodeDurationMs: (json['decode_duration_ms'] as num?)?.toDouble() ?? 0.0,
      presentationDelayMs: (json['presentation_delay_ms'] as num?)?.toDouble() ?? 0.0,
      endToEndLatencyMs: (json['end_to_end_latency_ms'] as num?)?.toDouble() ?? 0.0,
    );
  }
}

@immutable
class TelemetryStats {
  final double fps;
  final int bitrateBps;
  final double latencyMs;
  final double packetLossPercent;
  final int batteryPercent;
  final int temperatureCelsius;
  final int activeTransport;
  final int activeProfile;
  final LatencyBreakdown latencyBreakdown;

  const TelemetryStats({
    this.fps = 0.0,
    this.bitrateBps = 0,
    this.latencyMs = 0.0,
    this.packetLossPercent = 0.0,
    this.batteryPercent = 100,
    this.temperatureCelsius = 35,
    this.activeTransport = 0,
    this.activeProfile = 0,
    this.latencyBreakdown = const LatencyBreakdown(),
  });

  factory TelemetryStats.fromJson(Map<String, dynamic> json) {
    return TelemetryStats(
      fps: (json['fps'] as num?)?.toDouble() ?? 0.0,
      bitrateBps: (json['bitrate_bps'] as num?)?.toInt() ?? 0,
      latencyMs: (json['latency_ms'] as num?)?.toDouble() ?? 0.0,
      packetLossPercent: (json['packet_loss_percent'] as num?)?.toDouble() ?? 0.0,
      batteryPercent: (json['battery_percent'] as num?)?.toInt() ?? 100,
      temperatureCelsius: (json['temperature_celsius'] as num?)?.toInt() ?? 35,
      activeTransport: (json['active_transport'] as num?)?.toInt() ?? 0,
      activeProfile: (json['active_profile'] as num?)?.toInt() ?? 0,
      latencyBreakdown: json['latency_breakdown'] is Map<String, dynamic>
          ? LatencyBreakdown.fromJson(json['latency_breakdown'] as Map<String, dynamic>)
          : const LatencyBreakdown(),
    );
  }

  String get formattedBitrate {
    if (bitrateBps >= 1000000) {
      return '${(bitrateBps / 1000000).toStringAsFixed(1)} Mbps';
    }
    return '${(bitrateBps / 1000).toStringAsFixed(0)} kbps';
  }
}
