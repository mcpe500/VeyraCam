import 'dart:convert';
import 'package:veyra_models/veyra_models.dart';

class HandshakeMessage {
  static String createHello({
    required String deviceName,
    required String deviceId,
    required String model,
  }) {
    final payload = {
      'type': 'hello',
      'protocol': 1,
      'device': {
        'name': deviceName,
        'id': deviceId,
        'model': model,
      },
    };
    return jsonEncode(payload);
  }

  static String createStartStream({
    required StreamConfig config,
    String transport = 'auto',
  }) {
    final payload = {
      'type': 'startStream',
      'video': {
        'width': config.resolution.width,
        'height': config.resolution.height,
        'fps': config.fps,
        'bitrate': config.bitrateBps,
        'codec': 'h264',
        'profile': 'baseline',
      },
      'audio': config.enableAudio,
      'transport': transport,
    };
    return jsonEncode(payload);
  }

  static String createStopStream() {
    return jsonEncode({'type': 'stopStream'});
  }
}
