import 'dart:async';
import 'dart:convert';
import 'dart:io' show Platform;
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:veyra_models/veyra_models.dart';

class VeyraCamController extends ChangeNotifier {
  static const MethodChannel _controlChannel = MethodChannel('com.veyra.cam/control');
  static const EventChannel _telemetryChannel = EventChannel('com.veyra.cam/telemetry');

  int? _textureId;
  bool _isStreaming = false;
  bool _isConnecting = false;
  String? _pairingPin;
  String? _lastError;
  Timer? _pairingPinTimer;
  StreamSubscription? _telemetrySub;

  CameraControlsState _controls = const CameraControlsState();
  StreamProfile _profile = StreamProfile.balanced;
  TransportType _transport = TransportType.wifiLan;
  TelemetryStats _telemetry = const TelemetryStats();

  int? get textureId => _textureId;
  bool get isStreaming => _isStreaming;
  bool get isConnecting => _isConnecting;
  String? get pairingPin => _pairingPin;
  String? get lastError => _lastError;
  CameraControlsState get controls => _controls;
  StreamProfile get profile => _profile;
  TransportType get transport => _transport;
  TelemetryStats get telemetry => _telemetry;

  Future<bool> _requestPermissions() async {
    if (!Platform.isAndroid) return true;
    try {
      // Request in parallel; notification is Android 13+ only but handler handles it.
      final statuses = await [
        Permission.camera,
        Permission.microphone,
        Permission.notification,
      ].request();
      final camOk = statuses[Permission.camera]?.isGranted ?? false;
      final micOk = statuses[Permission.microphone]?.isGranted ?? false;
      // Notification is optional for foreground service, don't block on it.
      if (!camOk || !micOk) {
        _lastError = !camOk
            ? 'Camera permission denied — please allow Camera in Settings'
            : 'Microphone permission denied — please allow Microphone in Settings';
        // If permanently denied, guide to settings
        if ((await Permission.camera.isPermanentlyDenied) ||
            (await Permission.microphone.isPermanentlyDenied)) {
          _lastError = '$_lastError. Open App Settings to grant.';
        }
        return false;
      }
      return true;
    } catch (e) {
      debugPrint('Permission request failed: $e');
      // Fallback: assume granted and let native side guard — avoid blocking launch
      return true;
    }
  }

  Future<void> startStreaming({StreamProfile? customProfile}) async {
    if (_isStreaming || _isConnecting) return;
    _isConnecting = true;
    _lastError = null;
    notifyListeners();

    if (customProfile != null) {
      _profile = customProfile;
    }

    // Android runtime permissions (P0 crash fix — was missing entirely)
    if (Platform.isAndroid) {
      final granted = await _requestPermissions();
      if (!granted) {
        _isConnecting = false;
        notifyListeners();
        debugPrint('startStreaming aborted: $_lastError');
        return;
      }
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
      _lastError = null;
      _listenTelemetry();
      notifyListeners();
    } on PlatformException catch (e) {
      _isConnecting = false;
      _isStreaming = false;
      // Surface native error codes to UI
      if (e.code == 'PERMISSION_DENIED') {
        _lastError = 'Permission denied: ${e.message}';
      } else if (e.code == 'STREAM_FAILED' || e.code == 'TEXTURE_FAILED') {
        _lastError = 'Failed to start: ${e.message}';
      } else {
        _lastError = e.message ?? e.code;
      }
      notifyListeners();
      debugPrint('Failed to start streaming [${e.code}]: ${e.message}');
    } catch (e) {
      _isConnecting = false;
      _isStreaming = false;
      _lastError = e.toString();
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

    // C-2: poll the pairing PIN so the user can read it off the phone screen.
    _pairingPinTimer?.cancel();
    _pairingPinTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      _controlChannel.invokeMethod<String>('getPairingPin').then((pin) {
        if (_pairingPin != pin) {
          _pairingPin = pin;
          notifyListeners();
        }
      }).catchError((_) {});
    });
  }

  Future<void> stopStreaming() async {
    if (!_isStreaming) return;
    try {
      await _controlChannel.invokeMethod('stopStreaming');
    } catch (_) {}
    _telemetrySub?.cancel();
    _telemetrySub = null;
    _pairingPinTimer?.cancel();
    _pairingPinTimer = null;
    _pairingPin = null;
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

  void clearError() {
    _lastError = null;
    notifyListeners();
  }

  Future<void> openSettings() async {
    await openAppSettings();
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
    _pairingPinTimer?.cancel();
    super.dispose();
  }
}
