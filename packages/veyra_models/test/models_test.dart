import 'package:test/test.dart';
import 'package:veyra_models/veyra_models.dart';

void main() {
  group('Veyra Models Tests', () {
    test('Resolution label and equality', () {
      const res1 = Resolution(1280, 720);
      const res2 = Resolution(1280, 720);
      const res3 = Resolution(1920, 1080);

      expect(res1.label, equals('1280x720'));
      expect(res1, equals(res2));
      expect(res1, isNot(equals(res3)));
    });

    test('StreamProfile presets', () {
      expect(StreamProfile.presets.length, equals(6));
      expect(StreamProfile.balanced.resolution.width, equals(1280));
      expect(StreamProfile.balanced.fps, equals(30));
    });

    test('CameraControlsState copyWith and serialization', () {
      const state = CameraControlsState(zoom: 1.0, exposure: 0);
      final updated = state.copyWith(zoom: 2.5, exposure: 2);

      expect(updated.zoom, equals(2.5));
      expect(updated.exposure, equals(2));
      expect(updated.autoFocus, isTrue);

      final json = updated.toJson();
      expect(json['zoom'], equals(2.5));
      expect(json['exposure'], equals(2));
    });

    test('TelemetryStats parsing and formatting', () {
      final json = {
        'fps': 29.8,
        'bitrate_bps': 2450000,
        'latency_ms': 42.5,
        'packet_loss_percent': 0.005,
        'battery_percent': 85,
        'temperature_celsius': 38,
      };

      final stats = TelemetryStats.fromJson(json);
      expect(stats.fps, closeTo(29.8, 0.1));
      expect(stats.formattedBitrate, equals('2.5 Mbps'));
      expect(stats.batteryPercent, equals(85));
    });
  });
}
