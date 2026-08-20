import 'dart:convert';
import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';
import 'package:veyra_models/veyra_models.dart';
import 'ffi_bindings.dart';

class NativeDesktopSession {
  final VeyraNativeBindings _bindings;
  bool _isInitialized = false;
  bool _isConnected = false;

  ffi.NativeCallable<ffi.Void Function(ffi.Pointer<Utf8>)>? _statusCallable;

  NativeDesktopSession([VeyraNativeBindings? bindings])
      : _bindings = bindings ?? VeyraNativeBindings();

  bool get isConnected => _isConnected;

  bool initialize() {
    if (_isInitialized) return true;
    final res = _bindings.veyraCoreInit();
    _isInitialized = (res == 0);
    return _isInitialized;
  }

  // C-2: connect and perform out-of-band PIN pairing. [onStatus] receives
  // asynchronous status strings ("waiting_for_challenge", "paired",
  // "pairing_error:<reason>", ...). Pairing completes on the native thread and
  // is marshalled back to the Dart isolate.
  bool connectDevice(
    String hostIp, {
    int port = 5150,
    String pin = '',
    void Function(String status)? onStatus,
  }) {
    if (!_isInitialized) {
      initialize();
    }
    _disposeStatusCallable();

    ffi.NativeCallable<ffi.Void Function(ffi.Pointer<Utf8>)>? callable;
    if (onStatus != null) {
      callable = ffi.NativeCallable<ffi.Void Function(ffi.Pointer<Utf8>)>.listener(
        (ffi.Pointer<Utf8> statusPtr) {
          try {
            final status = statusPtr.toDartString();
            onStatus(status);
          } catch (_) {}
        },
      );
      _statusCallable = callable;
    }

    final hostPtr = hostIp.toNativeUtf8();
    final pinPtr = pin.toNativeUtf8();
    try {
      final res = _bindings.veyraCoreConnectDevice(
        hostPtr,
        port,
        pinPtr,
        callable?.nativeFunction ?? ffi.nullptr,
      );
      _isConnected = (res == 0);
      return _isConnected;
    } finally {
      calloc.free(hostPtr);
      calloc.free(pinPtr);
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
    _disposeStatusCallable();
    if (_isInitialized) {
      _bindings.veyraCoreShutdown();
      _isInitialized = false;
    }
  }

  void _disposeStatusCallable() {
    _statusCallable?.close();
    _statusCallable = null;
  }
}
