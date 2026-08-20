import 'package:meta/meta.dart';

@immutable
class CameraControlsState {
  final double zoom;
  final int exposure;
  final bool autoFocus;
  final double manualFocusDistance;
  final bool torch;
  final bool isFacingBack;

  const CameraControlsState({
    this.zoom = 1.0,
    this.exposure = 0,
    this.autoFocus = true,
    this.manualFocusDistance = 0.0,
    this.torch = false,
    this.isFacingBack = true,
  });

  CameraControlsState copyWith({
    double? zoom,
    int? exposure,
    bool? autoFocus,
    double? manualFocusDistance,
    bool? torch,
    bool? isFacingBack,
  }) {
    return CameraControlsState(
      zoom: zoom ?? this.zoom,
      exposure: exposure ?? this.exposure,
      autoFocus: autoFocus ?? this.autoFocus,
      manualFocusDistance: manualFocusDistance ?? this.manualFocusDistance,
      torch: torch ?? this.torch,
      isFacingBack: isFacingBack ?? this.isFacingBack,
    );
  }

  Map<String, dynamic> toJson() => {
        'zoom': zoom,
        'exposure': exposure,
        'autoFocus': autoFocus,
        'distance': manualFocusDistance,
        'torch': torch,
        'facingBack': isFacingBack,
      };
}
