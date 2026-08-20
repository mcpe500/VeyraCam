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

          // Error banner (permission / codec failure)
          if (c.lastError != null && !c.isStreaming)
            Positioned(
              top: 70,
              left: 16,
              right: 16,
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
                decoration: BoxDecoration(
                  color: VeyraColors.error.withAlpha(230),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: Colors.white24),
                ),
                child: Row(
                  children: [
                    const Icon(Icons.error_outline, color: Colors.white, size: 16),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        c.lastError!,
                        style: const TextStyle(color: Colors.white, fontSize: 12, fontWeight: FontWeight.w600),
                      ),
                    ),
                    if (c.lastError!.contains('Settings'))
                      TextButton(
                        onPressed: () => c.openSettings(),
                        style: TextButton.styleFrom(
                          padding: const EdgeInsets.symmetric(horizontal: 8),
                          minimumSize: Size.zero,
                          tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                        ),
                        child: const Text('Settings', style: TextStyle(color: Colors.white, fontSize: 12)),
                      ),
                    IconButton(
                      icon: const Icon(Icons.close, color: Colors.white70, size: 16),
                      padding: EdgeInsets.zero,
                      constraints: const BoxConstraints(),
                      onPressed: () => c.clearError(),
                    ),
                  ],
                ),
              ),
            ),

          // Diagnostic HUD
          if (c.isStreaming)
            Positioned(
              top: 70,
              left: 16,
              right: 16,
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  if (c.pairingPin != null)
                    Container(
                      margin: const EdgeInsets.only(bottom: 8),
                      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
                      decoration: BoxDecoration(
                        color: VeyraColors.primary,
                        borderRadius: BorderRadius.circular(12),
                        boxShadow: [
                          BoxShadow(
                            color: Colors.black.withAlpha(80),
                            blurRadius: 8,
                            offset: const Offset(0, 2),
                          ),
                        ],
                      ),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          const Icon(Icons.pin, color: Colors.black, size: 18),
                          const SizedBox(width: 8),
                          Text(
                            'Pairing PIN: ${c.pairingPin}',
                            style: const TextStyle(
                              color: Colors.black,
                              fontWeight: FontWeight.bold,
                              fontSize: 16,
                              letterSpacing: 2,
                            ),
                          ),
                        ],
                      ),
                    ),
                  DiagnosticHud(
                    stats: c.telemetry,
                    isExpanded: _isHudExpanded,
                    onToggleExpand: () {
                      setState(() => _isHudExpanded = !_isHudExpanded);
                    },
                  ),
                ],
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
