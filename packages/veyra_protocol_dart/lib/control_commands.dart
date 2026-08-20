import 'dart:convert';

class ControlCommands {
  static String setZoom(double zoomFactor) {
    return jsonEncode({
      'type': 'setZoom',
      'zoom': zoomFactor,
    });
  }

  static String setExposure(int exposureStep) {
    return jsonEncode({
      'type': 'setExposure',
      'exposure': exposureStep,
    });
  }

  static String setFocus({required bool autoFocus, double distance = 0.0}) {
    return jsonEncode({
      'type': 'setFocus',
      'autoFocus': autoFocus,
      'distance': distance,
    });
  }

  static String setTorch(bool enabled) {
    return jsonEncode({
      'type': 'setTorch',
      'enabled': enabled,
    });
  }

  static String switchCamera({required bool front}) {
    return jsonEncode({
      'type': 'switchCamera',
      'front': front,
    });
  }

  static String requestIdr() {
    return jsonEncode({
      'type': 'requestIdr',
    });
  }

  static String setBitrate(int bitrateBps) {
    return jsonEncode({
      'type': 'setBitrate',
      'bitrate': bitrateBps,
    });
  }

  static String ping(int clientTimestampUs) {
    return jsonEncode({
      'type': 'ping',
      'timestamp': clientTimestampUs,
    });
  }
}
