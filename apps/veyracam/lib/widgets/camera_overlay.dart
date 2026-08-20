import 'package:flutter/material.dart';
import 'package:veyra_models/veyra_models.dart';
import 'package:veyra_ui/veyra_ui.dart';
import '../controllers/camera_controller.dart';

class CameraOverlay extends StatefulWidget {
  final VeyraCamController controller;
  final VoidCallback onOpenSettings;

  const CameraOverlay({
    super.key,
    required this.controller,
    required this.onOpenSettings,
  });

  @override
  State<CameraOverlay> createState() => _CameraOverlayState();
}

class _CameraOverlayState extends State<CameraOverlay> {
  bool _isHudExpanded = false;
  bool _showManualDrawer = false;

  @override
  Widget build(BuildContext context) {
    final c = widget.controller;

    return SafeArea(
      child: Stack(
        children: [
          // Top Control Bar
          Positioned(
            top: 12,
            left: 16,
            right: 16,
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                StatusBadge(
                  transportType: c.transport,
                  status: c.isStreaming
                      ? ConnectionStatus.streaming
                      : ConnectionStatus.disconnected,
                ),
                Row(
                  children: [
                    IconButton(
                      icon: Icon(
                        c.controls.torch ? Icons.flash_on : Icons.flash_off,
                        color: c.controls.torch ? VeyraColors.warning : Colors.white70,
                      ),
                      onPressed: () => c.setTorch(!c.controls.torch),
                    ),
                    IconButton(
                      icon: const Icon(Icons.flip_camera_android, color: Colors.white70),
                      onPressed: c.switchCamera,
                    ),
                    IconButton(
                      icon: const Icon(Icons.tune, color: Colors.white70),
                      onPressed: () {
                        setState(() => _showManualDrawer = !_showManualDrawer);
                      },
                    ),
                    IconButton(
                      icon: const Icon(Icons.settings, color: Colors.white70),
                      onPressed: widget.onOpenSettings,
                    ),
                  ],
                ),
              ],
            ),
          ),

          // Diagnostic HUD
          if (c.isStreaming)
            Positioned(
              top: 70,
              left: 16,
              right: 16,
              child: DiagnosticHud(
                stats: c.telemetry,
                isExpanded: _isHudExpanded,
                onToggleExpand: () {
                  setState(() => _isHudExpanded = !_isHudExpanded);
                },
              ),
            ),

          // Manual 3A Drawer
          if (_showManualDrawer)
            Positioned(
              bottom: 140,
              left: 16,
              right: 16,
              child: Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: VeyraColors.surface.withAlpha(240),
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(color: VeyraColors.border),
                ),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    ExposureSlider(
                      exposure: c.controls.exposure,
                      onChanged: (val) => c.setExposure(val),
                    ),
                    const SizedBox(height: 8),
                    ManualFocusControl(
                      autoFocus: c.controls.autoFocus,
                      distance: c.controls.manualFocusDistance,
                      onToggleAutoFocus: (val) => c.setFocus(autoFocus: val),
                      onDistanceChanged: (val) => c.setFocus(autoFocus: false, distance: val),
                    ),
                  ],
                ),
              ),
            ),

          // Zoom Quick Presets & Slider
          Positioned(
            bottom: 80,
            left: 24,
            right: 24,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [1.0, 2.0, 5.0].map((preset) {
                    final isSelected = (c.controls.zoom - preset).abs() < 0.2;
                    return Padding(
                      padding: const EdgeInsets.symmetric(horizontal: 6),
                      child: ChoiceChip(
                        label: Text('${preset.toInt()}x'),
                        selected: isSelected,
                        selectedColor: VeyraColors.primary,
                        backgroundColor: VeyraColors.surfaceElevated,
                        labelStyle: TextStyle(
                          color: isSelected ? Colors.black : Colors.white70,
                          fontWeight: FontWeight.bold,
                          fontSize: 12,
                        ),
                        onSelected: (_) => c.setZoom(preset),
                      ),
                    );
                  }).toList(),
                ),
                const SizedBox(height: 4),
                ZoomSlider(
                  zoom: c.controls.zoom,
                  maxZoom: 8.0,
                  onChanged: (val) => c.setZoom(val),
                ),
              ],
            ),
          ),

          // Bottom Streaming Action Button
          Positioned(
            bottom: 16,
            left: 32,
            right: 32,
            child: ElevatedButton(
              onPressed: c.isConnecting
                  ? null
                  : () {
                      if (c.isStreaming) {
                        c.stopStreaming();
                      } else {
                        c.startStreaming();
                      }
                    },
              style: ElevatedButton.styleFrom(
                backgroundColor: c.isStreaming ? VeyraColors.error : VeyraColors.primary,
                foregroundColor: c.isStreaming ? Colors.white : Colors.black,
                padding: const EdgeInsets.symmetric(vertical: 14),
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
              ),
              child: Text(
                c.isConnecting
                    ? 'STARTING STREAM...'
                    : (c.isStreaming ? 'STOP STREAMING' : 'START STREAMING'),
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 14, letterSpacing: 0.5),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
