import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:veyra_models/veyra_models.dart';
import 'package:veyra_native/veyra_native.dart';

class DesktopSessionController extends ChangeNotifier {
  late final NativeDesktopSession _session;
  Timer? _telemetryTimer;

  bool _isConnecting = false;
  bool _isConnected = false;
  String? _connectedDeviceIp;
  String? _pairingStatus;

  CameraControlsState _controls = const CameraControlsState();
  final StreamProfile _profile = StreamProfile.balanced;
  final TransportType _activeTransport = TransportType.wifiLan;
  TelemetryStats _telemetry = const TelemetryStats();

  bool get isConnecting => _isConnecting;
  bool get isConnected => _isConnected;
  String? get connectedDeviceIp => _connectedDeviceIp;
  String? get pairingStatus => _pairingStatus;
  CameraControlsState get controls => _controls;
  StreamProfile get profile => _profile;
  TransportType get activeTransport => _activeTransport;
  TelemetryStats get telemetry => _telemetry;

  DesktopSessionController([NativeDesktopSession? session]) {
    _session = session ?? NativeDesktopSession();
    _session.initialize();
  }

  Future<bool> connectDevice(String hostIp,
      {int port = 5150, String pin = ''}) async {
    if (_isConnecting || _isConnected) return false;
    _isConnecting = true;
    _pairingStatus = 'connecting';
    notifyListeners();

    try {
      final success = _session.connectDevice(
        hostIp,
        port: port,
        pin: pin,
        onStatus: _handlePairingStatus,
      );
      _isConnected = success;
      _isConnecting = false;
      if (success) {
        _connectedDeviceIp = hostIp;
        _pairingStatus = 'paired';
        _startTelemetryPolling();
      }
      notifyListeners();
      return success;
    } catch (e) {
      _isConnecting = false;
      _isConnected = false;
      _pairingStatus = 'error';
      notifyListeners();
      debugPrint('Failed to connect: $e');
      return false;
    }
  }

  // Called from the FFI status callback (marshalled onto the isolate loop).
  void _handlePairingStatus(String status) {
    _pairingStatus = status;
    if (status == 'paired' && !_isConnected) {
      _isConnected = true;
      _isConnecting = false;
      _startTelemetryPolling();
    }
    if (status.startsWith('pairing_error')) {
      _isConnecting = false;
      _isConnected = false;
    }
    notifyListeners();
  }

  void disconnectDevice() {
    _telemetryTimer?.cancel();
    _telemetryTimer = null;
    _session.disconnectDevice();
    _isConnected = false;
    _connectedDeviceIp = null;
    _telemetry = const TelemetryStats();
    notifyListeners();
  }

  void setZoom(double zoom) {
    _controls = _controls.copyWith(zoom: zoom);
    notifyListeners();
    _session.setZoom(zoom);
  }

  void setExposure(int exposure) {
    _controls = _controls.copyWith(exposure: exposure);
    notifyListeners();
    _session.setExposure(exposure);
  }

  void setFocus({required bool autoFocus, double distance = 0.0}) {
    _controls = _controls.copyWith(autoFocus: autoFocus, manualFocusDistance: distance);
    notifyListeners();
    _session.setFocus(autoFocus: autoFocus, distance: distance);
  }

  void setTorch(bool enabled) {
    _controls = _controls.copyWith(torch: enabled);
    notifyListeners();
    _session.setTorch(enabled);
  }

  void switchCamera() {
    final nextFacingFront = _controls.isFacingBack; // toggles to front
    _controls = _controls.copyWith(isFacingBack: !nextFacingFront);
    notifyListeners();
    _session.switchCamera(front: nextFacingFront);
  }

  void requestIdr() {
    _session.requestIdr();
  }

  void _startTelemetryPolling() {
    _telemetryTimer?.cancel();
    _telemetryTimer = Timer.periodic(const Duration(milliseconds: 300), (_) {
      if (_isConnected) {
        final stats = _session.getTelemetry();
        _telemetry = stats;
        notifyListeners();
      }
    });
  }

  @override
  void dispose() {
    _telemetryTimer?.cancel();
    _session.shutdown();
    super.dispose();
  }
}
