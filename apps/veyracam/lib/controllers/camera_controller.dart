import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:veyra_models/veyra_models.dart';

class VeyraCamController extends ChangeNotifier {
  static const MethodChannel _controlChannel = MethodChannel('com.veyra.cam/control');
  static const EventChannel _telemetryChannel = EventChannel('com.veyra.cam/telemetry');

  int? _textureId;
  bool _isStreaming = false;
  bool _isConnecting = false;
  StreamSubscription? _telemetrySub;

  CameraControlsState _controls = const CameraControlsState();
  StreamProfile _profile = StreamProfile.balanced;
  TransportType _transport = TransportType.wifiLan;
  TelemetryStats _telemetry = const TelemetryStats();

  int? get textureId => _textureId;
  bool get isStreaming => _isStreaming;
  bool get isConnecting => _isConnecting;
  CameraControlsState get controls => _controls;
  StreamProfile get profile => _profile;
  TransportType get transport => _transport;
  TelemetryStats get telemetry => _telemetry;

  Future<void> startStreaming({StreamProfile? customProfile}) async {
    if (_isStreaming || _isConnecting) return;
    _isConnecting = true;
    notifyListeners();

    if (customProfile != null) {
      _profile = customProfile;
    }

    try {
      final res = await _controlChannel.invokeMethod<Map>('startStreaming', {
        'facingBack': _controls.isFacingBack,
        'width': _profile.resolution.width,
        'height': _profile.resolution.height,
        'fps': _profile.fps,
        'bitrate': _profile.bitrateBps,
      });

      if (res != null && res.containsKey('textureId')) {
        _textureId = (res['textureId'] as num).toInt();
      }

      _isStreaming = true;
      _isConnecting = false;
      _listenTelemetry();
      notifyListeners();
    } catch (e) {
      _isConnecting = false;
      _isStreaming = false;
      notifyListeners();
      debugPrint('Failed to start streaming: $e');
    }
  }

  void _listenTelemetry() {
    _telemetrySub?.cancel();
    _telemetrySub = _telemetryChannel.receiveBroadcastStream().listen((data) {
      if (data is String && data.isNotEmpty && data != '{}') {
        try {
          final map = jsonDecode(data) as Map<String, dynamic>;
          _telemetry = TelemetryStats.fromJson(map);
          notifyListeners();
        } catch (_) {}
      }
    });
  }

  Future<void> stopStreaming() async {
    if (!_isStreaming) return;
    try {
      await _controlChannel.invokeMethod('stopStreaming');
    } catch (_) {}
    _telemetrySub?.cancel();
    _telemetrySub = null;
    _isStreaming = false;
    _textureId = null;
    notifyListeners();
  }

  Future<void> setZoom(double zoom) async {
    _controls = _controls.copyWith(zoom: zoom);
    notifyListeners();
    try {
      await _controlChannel.invokeMethod('setZoom', {'zoom': zoom});
    } catch (_) {}
  }

  Future<void> setExposure(int exposure) async {
    _controls = _controls.copyWith(exposure: exposure);
    notifyListeners();
    try {
      await _controlChannel.invokeMethod('setExposure', {'exposure': exposure});
    } catch (_) {}
  }

  Future<void> setFocus({required bool autoFocus, double distance = 0.0}) async {
    _controls = _controls.copyWith(autoFocus: autoFocus, manualFocusDistance: distance);
    notifyListeners();
    try {
      await _controlChannel.invokeMethod('setFocus', {
        'autoFocus': autoFocus,
        'distance': distance,
      });
    } catch (_) {}
  }

  Future<void> setTorch(bool enabled) async {
    _controls = _controls.copyWith(torch: enabled);
    notifyListeners();
    try {
      await _controlChannel.invokeMethod('setTorch', {'enabled': enabled});
    } catch (_) {}
  }

  Future<void> switchCamera() async {
    _controls = _controls.copyWith(isFacingBack: !_controls.isFacingBack);
    notifyListeners();
    try {
      await _controlChannel.invokeMethod('switchCamera');
    } catch (_) {}
  }

  Future<void> requestIdr() async {
    try {
      await _controlChannel.invokeMethod('requestIdr');
    } catch (_) {}
  }

  void setProfile(StreamProfile newProfile) {
    _profile = newProfile;
    notifyListeners();
    if (_isStreaming) {
      // Re-start or update bitrate
      _controlChannel.invokeMethod('setBitrate', {'bitrate': newProfile.bitrateBps});
    }
  }

  @override
  void dispose() {
    _telemetrySub?.cancel();
    super.dispose();
  }
}
