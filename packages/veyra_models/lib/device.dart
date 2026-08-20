import 'package:meta/meta.dart';

@immutable
class Resolution {
  final int width;
  final int height;

  const Resolution(this.width, this.height);

  String get label => '${width}x$height';

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is Resolution &&
          runtimeType == other.runtimeType &&
          width == other.width &&
          height == other.height;

  @override
  int get hashCode => width.hashCode ^ height.hashCode;

  @override
  String toString() => label;
}

@immutable
class CameraSpec {
  final String cameraId;
  final bool isFacingBack;
  final List<Resolution> supportedResolutions;
  final List<int> supportedFps;
  final double maxZoom;
  final bool hasFlash;
  final bool supportsManualFocus;
  final int minExposure;
  final int maxExposure;

  const CameraSpec({
    required this.cameraId,
    required this.isFacingBack,
    required this.supportedResolutions,
    required this.supportedFps,
    required this.maxZoom,
    required this.hasFlash,
    required this.supportsManualFocus,
    required this.minExposure,
    required this.maxExposure,
  });

  factory CameraSpec.fromJson(Map<String, dynamic> json) {
    final resolutions = (json['resolutions'] as List? ?? [])
        .map((r) {
          final parts = r.toString().split('x');
          if (parts.length == 2) {
            return Resolution(int.tryParse(parts[0]) ?? 1280, int.tryParse(parts[1]) ?? 720);
          }
          return const Resolution(1280, 720);
        })
        .toList();

    return CameraSpec(
      cameraId: json['id']?.toString() ?? '0',
      isFacingBack: json['facing'] == 'back',
      supportedResolutions: resolutions,
      supportedFps: [15, 24, 30, 60],
      maxZoom: (json['max_zoom'] as num?)?.toDouble() ?? 1.0,
      hasFlash: json['has_flash'] as bool? ?? false,
      supportsManualFocus: json['manual_focus'] as bool? ?? false,
      minExposure: json['min_exposure'] as int? ?? -4,
      maxExposure: json['max_exposure'] as int? ?? 4,
    );
  }
}

@immutable
class Device {
  final String id;
  final String name;
  final String model;
  final String ipAddress;
  final int port;
  final List<CameraSpec> cameras;
  final bool isPaired;

  const Device({
    required this.id,
    required this.name,
    required this.model,
    required this.ipAddress,
    this.port = 5150,
    required this.cameras,
    this.isPaired = false,
  });

  Device copyWith({
    String? id,
    String? name,
    String? model,
    String? ipAddress,
    int? port,
    List<CameraSpec>? cameras,
    bool? isPaired,
  }) {
    return Device(
      id: id ?? this.id,
      name: name ?? this.name,
      model: model ?? this.model,
      ipAddress: ipAddress ?? this.ipAddress,
      port: port ?? this.port,
      cameras: cameras ?? this.cameras,
      isPaired: isPaired ?? this.isPaired,
    );
  }
}
