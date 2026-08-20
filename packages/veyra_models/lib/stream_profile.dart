import 'package:meta/meta.dart';
import 'device.dart';

enum StreamProfileId {
  ultraLow,   // 320x240 @ 15fps, 200 kbps
  bluetooth,  // 640x360 @ 15fps, 400 kbps
  low,        // 640x480 @ 24fps, 700 kbps
  balanced,   // 1280x720 @ 30fps, 2.5 Mbps (Default)
  high,       // 1920x1080 @ 30fps, 4.5 Mbps
  performance // 1920x1080 @ 60fps, 8.0 Mbps
}

@immutable
class StreamProfile {
  final StreamProfileId id;
  final String name;
  final Resolution resolution;
  final int fps;
  final int bitrateBps;

  const StreamProfile({
    required this.id,
    required this.name,
    required this.resolution,
    required this.fps,
    required this.bitrateBps,
  });

  static const ultraLow = StreamProfile(
    id: StreamProfileId.ultraLow,
    name: 'Ultra Low',
    resolution: Resolution(320, 240),
    fps: 15,
    bitrateBps: 200000,
  );

  static const bluetooth = StreamProfile(
    id: StreamProfileId.bluetooth,
    name: 'Bluetooth Adaptive',
    resolution: Resolution(640, 360),
    fps: 15,
    bitrateBps: 400000,
  );

  static const low = StreamProfile(
    id: StreamProfileId.low,
    name: 'Low',
    resolution: Resolution(640, 480),
    fps: 24,
    bitrateBps: 700000,
  );

  static const balanced = StreamProfile(
    id: StreamProfileId.balanced,
    name: 'Balanced (720p30)',
    resolution: Resolution(1280, 720),
    fps: 30,
    bitrateBps: 2500000,
  );

  static const high = StreamProfile(
    id: StreamProfileId.high,
    name: 'High (1080p30)',
    resolution: Resolution(1920, 1080),
    fps: 30,
    bitrateBps: 4500000,
  );

  static const performance = StreamProfile(
    id: StreamProfileId.performance,
    name: 'Performance (1080p60)',
    resolution: Resolution(1920, 1080),
    fps: 60,
    bitrateBps: 8000000,
  );

  static const List<StreamProfile> presets = [
    ultraLow,
    bluetooth,
    low,
    balanced,
    high,
    performance,
  ];
}

@immutable
class StreamConfig {
  final Resolution resolution;
  final int fps;
  final int bitrateBps;
  final StreamProfileId profileId;
  final bool enableAudio;
  final bool enableEncryption;

  const StreamConfig({
    this.resolution = const Resolution(1280, 720),
    this.fps = 30,
    this.bitrateBps = 2500000,
    this.profileId = StreamProfileId.balanced,
    this.enableAudio = true,
    this.enableEncryption = false,
  });

  Map<String, dynamic> toJson() => {
        'width': resolution.width,
        'height': resolution.height,
        'fps': fps,
        'bitrate': bitrateBps,
        'profile': profileId.index,
        'audio': enableAudio,
        'encryption': enableEncryption,
      };
}
