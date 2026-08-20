import 'dart:convert';
import 'package:ffi/ffi.dart';
import 'package:veyra_models/veyra_models.dart';
import 'ffi_bindings.dart';

class NativeDesktopSession {
  final VeyraNativeBindings _bindings;
  bool _isInitialized = false;
  bool _isConnected = false;

  NativeDesktopSession([VeyraNativeBindings? bindings])
      : _bindings = bindings ?? VeyraNativeBindings();

  bool get isConnected => _isConnected;

  bool initialize() {
    if (_isInitialized) return true;
    final res = _bindings.veyraCoreInit();
    _isInitialized = (res == 0);
    return _isInitialized;
  }

  bool connectDevice(String hostIp, [int port = 5150]) {
    if (!_isInitialized) {
      initialize();
    }
    final hostPtr = hostIp.toNativeUtf8();
    try {
      final res = _bindings.veyraCoreConnectDevice(hostPtr, port);
      _isConnected = (res == 0);
      return _isConnected;
    } finally {
      calloc.free(hostPtr);
    }
  }

  void disconnectDevice() {
    if (_isConnected) {
      _bindings.veyraCoreDisconnectDevice();
      _isConnected = false;
    }
  }

  void setZoom(double zoom) {
    if (_isConnected) {
      _bindings.veyraCoreSetZoom(zoom);
    }
  }

  void setExposure(int exposure) {
    if (_isConnected) {
      _bindings.veyraCoreSetExposure(exposure);
    }
  }

  void setFocus({required bool autoFocus, double distance = 0.0}) {
    if (_isConnected) {
      _bindings.veyraCoreSetFocus(autoFocus ? 1 : 0, distance);
    }
  }

  void setTorch(bool enabled) {
    if (_isConnected) {
      _bindings.veyraCoreSetTorch(enabled ? 1 : 0);
    }
  }

  void switchCamera({required bool front}) {
    if (_isConnected) {
      _bindings.veyraCoreSwitchCamera(front ? 1 : 0);
    }
  }

  void requestIdr() {
    if (_isConnected) {
      _bindings.veyraCoreRequestIdr();
    }
  }

  int getSharedTextureHandle() {
    return _bindings.veyraCoreGetSharedTextureHandle();
  }

  TelemetryStats getTelemetry() {
    final ptr = _bindings.veyraCoreGetTelemetryJson();
    if (ptr.address == 0) return const TelemetryStats();

    try {
      final str = ptr.toDartString();
      if (str.isEmpty || str == '{}') return const TelemetryStats();
      final map = jsonDecode(str) as Map<String, dynamic>;
      return TelemetryStats.fromJson(map);
    } catch (_) {
      return const TelemetryStats();
    } finally {
      _bindings.veyraCoreFreeString(ptr);
    }
  }

  void shutdown() {
    disconnectDevice();
    if (_isInitialized) {
      _bindings.veyraCoreShutdown();
      _isInitialized = false;
    }
  }
}
