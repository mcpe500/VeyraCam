import 'package:flutter/material.dart';
import 'theme.dart';

class ZoomSlider extends StatelessWidget {
  final double zoom;
  final double maxZoom;
  final ValueChanged<double> onChanged;

  const ZoomSlider({
    super.key,
    required this.zoom,
    this.maxZoom = 8.0,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        const Icon(Icons.zoom_in, color: VeyraColors.primary, size: 20),
        const SizedBox(width: 8),
        Expanded(
          child: SliderTheme(
            data: SliderTheme.of(context).copyWith(
              activeTrackColor: VeyraColors.primary,
              inactiveTrackColor: VeyraColors.surfaceElevated,
              thumbColor: VeyraColors.primary,
              overlayColor: VeyraColors.primary.withAlpha(50),
              trackHeight: 4,
            ),
            child: Slider(
              value: zoom.clamp(1.0, maxZoom),
              min: 1.0,
              max: maxZoom,
              onChanged: onChanged,
            ),
          ),
        ),
        SizedBox(
          width: 45,
          child: Text(
            '${zoom.toStringAsFixed(1)}x',
            style: const TextStyle(
              color: VeyraColors.primary,
              fontWeight: FontWeight.bold,
              fontSize: 12,
            ),
          ),
        ),
      ],
    );
  }
}

class ExposureSlider extends StatelessWidget {
  final int exposure;
  final int minExposure;
  final int maxExposure;
  final ValueChanged<int> onChanged;

  const ExposureSlider({
    super.key,
    required this.exposure,
    this.minExposure = -4,
    this.maxExposure = 4,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        const Icon(Icons.brightness_6, color: VeyraColors.warning, size: 20),
        const SizedBox(width: 8),
        Expanded(
          child: SliderTheme(
            data: SliderTheme.of(context).copyWith(
              activeTrackColor: VeyraColors.warning,
              inactiveTrackColor: VeyraColors.surfaceElevated,
              thumbColor: VeyraColors.warning,
              overlayColor: VeyraColors.warning.withAlpha(50),
              trackHeight: 4,
            ),
            child: Slider(
              value: exposure.toDouble().clamp(minExposure.toDouble(), maxExposure.toDouble()),
              min: minExposure.toDouble(),
              max: maxExposure.toDouble(),
              divisions: maxExposure - minExposure,
              onChanged: (val) => onChanged(val.round()),
            ),
          ),
        ),
        SizedBox(
          width: 45,
          child: Text(
            '${exposure > 0 ? "+$exposure" : exposure} EV',
            style: const TextStyle(
              color: VeyraColors.warning,
              fontWeight: FontWeight.bold,
              fontSize: 12,
            ),
          ),
        ),
      ],
    );
  }
}

class ManualFocusControl extends StatelessWidget {
  final bool autoFocus;
  final double distance;
  final ValueChanged<bool> onToggleAutoFocus;
  final ValueChanged<double> onDistanceChanged;

  const ManualFocusControl({
    super.key,
    required this.autoFocus,
    required this.distance,
    required this.onToggleAutoFocus,
    required this.onDistanceChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        InkWell(
          onTap: () => onToggleAutoFocus(!autoFocus),
          borderRadius: BorderRadius.circular(6),
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
            decoration: BoxDecoration(
              color: autoFocus ? VeyraColors.primary : VeyraColors.surfaceElevated,
              borderRadius: BorderRadius.circular(6),
            ),
            child: Text(
              autoFocus ? 'AF' : 'MF',
              style: TextStyle(
                color: autoFocus ? Colors.black : VeyraColors.textSecondary,
                fontWeight: FontWeight.bold,
                fontSize: 11,
              ),
            ),
          ),
        ),
        const SizedBox(width: 8),
        Expanded(
          child: Opacity(
            opacity: autoFocus ? 0.4 : 1.0,
            child: SliderTheme(
              data: SliderTheme.of(context).copyWith(
                activeTrackColor: VeyraColors.accent,
                inactiveTrackColor: VeyraColors.surfaceElevated,
                thumbColor: VeyraColors.accent,
                trackHeight: 4,
              ),
              child: Slider(
                value: distance.clamp(0.0, 1.0),
                min: 0.0,
                max: 1.0,
                onChanged: autoFocus ? null : onDistanceChanged,
              ),
            ),
          ),
        ),
        SizedBox(
          width: 45,
          child: Text(
            autoFocus ? 'Auto' : '${(distance * 100).toInt()}%',
            style: const TextStyle(
              color: VeyraColors.accent,
              fontWeight: FontWeight.bold,
              fontSize: 12,
            ),
          ),
        ),
      ],
    );
  }
}
