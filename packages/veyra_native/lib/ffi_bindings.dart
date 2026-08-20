import 'dart:ffi' as ffi;
import 'dart:io';
import 'package:ffi/ffi.dart';

typedef VeyraCoreInitC = ffi.Int32 Function();
typedef VeyraCoreInitDart = int Function();

typedef VeyraCoreConnectStatusCallbackC = ffi.Void Function(ffi.Pointer<Utf8> status);
typedef VeyraCoreConnectStatusCallbackDart = void Function(ffi.Pointer<Utf8> status);

typedef VeyraCoreConnectDeviceC = ffi.Int32 Function(
    ffi.Pointer<Utf8> hostIp, ffi.Uint16 port, ffi.Pointer<Utf8> pin,
    ffi.Pointer<ffi.NativeFunction<VeyraCoreConnectStatusCallbackC>> statusCallback);
typedef VeyraCoreConnectDeviceDart = int Function(
    ffi.Pointer<Utf8> hostIp, int port, ffi.Pointer<Utf8> pin,
    ffi.Pointer<ffi.NativeFunction<VeyraCoreConnectStatusCallbackC>> statusCallback);

typedef VeyraCoreDisconnectDeviceC = ffi.Void Function();
typedef VeyraCoreDisconnectDeviceDart = void Function();

typedef VeyraCoreSetZoomC = ffi.Void Function(ffi.Float zoom);
typedef VeyraCoreSetZoomDart = void Function(double zoom);

typedef VeyraCoreSetExposureC = ffi.Void Function(ffi.Int32 exposure);
typedef VeyraCoreSetExposureDart = void Function(int exposure);

typedef VeyraCoreSetFocusC = ffi.Void Function(ffi.Int32 autoFocus, ffi.Float distance);
typedef VeyraCoreSetFocusDart = void Function(int autoFocus, double distance);

typedef VeyraCoreSetTorchC = ffi.Void Function(ffi.Int32 enabled);
typedef VeyraCoreSetTorchDart = void Function(int enabled);

typedef VeyraCoreSwitchCameraC = ffi.Void Function(ffi.Int32 front);
typedef VeyraCoreSwitchCameraDart = void Function(int front);

typedef VeyraCoreRequestIdrC = ffi.Void Function();
typedef VeyraCoreRequestIdrDart = void Function();

typedef VeyraCoreGetSharedTextureHandleC = ffi.Uint64 Function();
typedef VeyraCoreGetSharedTextureHandleDart = int Function();

typedef VeyraCoreGetTelemetryJsonC = ffi.Pointer<Utf8> Function();
typedef VeyraCoreGetTelemetryJsonDart = ffi.Pointer<Utf8> Function();

typedef VeyraCoreFreeStringC = ffi.Void Function(ffi.Pointer<Utf8> str);
typedef VeyraCoreFreeStringDart = void Function(ffi.Pointer<Utf8> str);

typedef VeyraCoreShutdownC = ffi.Void Function();
typedef VeyraCoreShutdownDart = void Function();

class VeyraNativeBindings {
  late final ffi.DynamicLibrary _dylib;

  late final VeyraCoreInitDart veyraCoreInit;
  late final VeyraCoreConnectDeviceDart veyraCoreConnectDevice;
  late final VeyraCoreDisconnectDeviceDart veyraCoreDisconnectDevice;
  late final VeyraCoreSetZoomDart veyraCoreSetZoom;
  late final VeyraCoreSetExposureDart veyraCoreSetExposure;
  late final VeyraCoreSetFocusDart veyraCoreSetFocus;
  late final VeyraCoreSetTorchDart veyraCoreSetTorch;
  late final VeyraCoreSwitchCameraDart veyraCoreSwitchCamera;
  late final VeyraCoreRequestIdrDart veyraCoreRequestIdr;
  late final VeyraCoreGetSharedTextureHandleDart veyraCoreGetSharedTextureHandle;
  late final VeyraCoreGetTelemetryJsonDart veyraCoreGetTelemetryJson;
  late final VeyraCoreFreeStringDart veyraCoreFreeString;
  late final VeyraCoreShutdownDart veyraCoreShutdown;

  VeyraNativeBindings([String? dynamicLibraryPath]) {
    final libraryPath = dynamicLibraryPath ?? _resolveDefaultLibraryPath();
    _dylib = ffi.DynamicLibrary.open(libraryPath);

    veyraCoreInit = _dylib
        .lookupFunction<VeyraCoreInitC, VeyraCoreInitDart>('veyra_core_init');
    veyraCoreConnectDevice = _dylib
        .lookupFunction<VeyraCoreConnectDeviceC, VeyraCoreConnectDeviceDart>('veyra_core_connect_device');
    veyraCoreDisconnectDevice = _dylib
        .lookupFunction<VeyraCoreDisconnectDeviceC, VeyraCoreDisconnectDeviceDart>('veyra_core_disconnect_device');
    veyraCoreSetZoom = _dylib
        .lookupFunction<VeyraCoreSetZoomC, VeyraCoreSetZoomDart>('veyra_core_set_zoom');
    veyraCoreSetExposure = _dylib
        .lookupFunction<VeyraCoreSetExposureC, VeyraCoreSetExposureDart>('veyra_core_set_exposure');
    veyraCoreSetFocus = _dylib
        .lookupFunction<VeyraCoreSetFocusC, VeyraCoreSetFocusDart>('veyra_core_set_focus');
    veyraCoreSetTorch = _dylib
        .lookupFunction<VeyraCoreSetTorchC, VeyraCoreSetTorchDart>('veyra_core_set_torch');
    veyraCoreSwitchCamera = _dylib
        .lookupFunction<VeyraCoreSwitchCameraC, VeyraCoreSwitchCameraDart>('veyra_core_switch_camera');
    veyraCoreRequestIdr = _dylib
        .lookupFunction<VeyraCoreRequestIdrC, VeyraCoreRequestIdrDart>('veyra_core_request_idr');
    veyraCoreGetSharedTextureHandle = _dylib
        .lookupFunction<VeyraCoreGetSharedTextureHandleC, VeyraCoreGetSharedTextureHandleDart>('veyra_core_get_shared_texture_handle');
    veyraCoreGetTelemetryJson = _dylib
        .lookupFunction<VeyraCoreGetTelemetryJsonC, VeyraCoreGetTelemetryJsonDart>('veyra_core_get_telemetry_json');
    veyraCoreFreeString = _dylib
        .lookupFunction<VeyraCoreFreeStringC, VeyraCoreFreeStringDart>('veyra_core_free_string');
    veyraCoreShutdown = _dylib
        .lookupFunction<VeyraCoreShutdownC, VeyraCoreShutdownDart>('veyra_core_shutdown');
  }

  static String _resolveDefaultLibraryPath() {
    if (Platform.isWindows) {
      return 'veyra_core_api.dll';
    } else if (Platform.isLinux) {
      return './native/windows/build/libveyra_core_api.so';
    } else if (Platform.isMacOS) {
      return 'libveyra_core_api.dylib';
    }
    return 'libveyra_core_api.so';
  }
}
