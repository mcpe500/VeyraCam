import 'package:flutter/material.dart';
import 'package:veyra_models/veyra_models.dart';
import 'package:veyra_ui/veyra_ui.dart';
import '../controllers/desktop_session_controller.dart';
import 'device_discovery_dialog.dart';

class DesktopMainScreen extends StatefulWidget {
  const DesktopMainScreen({super.key});

  @override
  State<DesktopMainScreen> createState() => _DesktopMainScreenState();
}

class _DesktopMainScreenState extends State<DesktopMainScreen> {
  final DesktopSessionController _controller = DesktopSessionController();
  bool _isHudExpanded = true;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Row(
          children: [
            Icon(Icons.camera_alt, color: VeyraColors.primary, size: 22),
            SizedBox(width: 10),
            Text('VeyraLink — Desktop Controller'),
          ],
        ),
        actions: [
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
            margin: const EdgeInsets.only(right: 16),
            decoration: BoxDecoration(
              color: VeyraColors.surfaceElevated,
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: VeyraColors.border),
            ),
            child: const Row(
              children: [
                Icon(Icons.videocam, color: VeyraColors.success, size: 16),
                SizedBox(width: 8),
                Text(
                  'Veyra Camera active in Zoom / OBS / Teams',
                  style: TextStyle(color: VeyraColors.textPrimary, fontSize: 12),
                ),
              ],
            ),
          ),
        ],
      ),
      body: ListenableBuilder(
        listenable: _controller,
        builder: (context, _) {
          return Column(
            children: [
              // Main Workspace Split: Left Controls, Center Preview, Right 3A Controls
              Expanded(
                child: Row(
                  children: [
                    // Left Panel: Device & Connection
                    _buildLeftPanel(),

                    // Center Viewport: Video Preview
                    Expanded(child: _buildCenterPreview()),

                    // Right Panel: Remote 3A Camera Controls
                    _buildRightControls(),
                  ],
                ),
              ),

              // Bottom Telemetry HUD
              if (_controller.isConnected)
                Container(
                  color: VeyraColors.surface,
                  padding: const EdgeInsets.all(12),
                  child: DiagnosticHud(
                    stats: _controller.telemetry,
                    isExpanded: _isHudExpanded,
                    onToggleExpand: () {
                      setState(() => _isHudExpanded = !_isHudExpanded);
                    },
                  ),
                ),
            ],
          );
        },
      ),
    );
  }

  Widget _buildLeftPanel() {
    return Container(
      width: 280,
      decoration: const BoxDecoration(
        color: VeyraColors.surface,
        border: Border(right: BorderSide(color: VeyraColors.border)),
      ),
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'CONNECTION & DEVICE',
            style: TextStyle(
              color: VeyraColors.textSecondary,
              fontSize: 11,
              fontWeight: FontWeight.bold,
              letterSpacing: 1.0,
            ),
          ),
          const SizedBox(height: 12),
          StatusBadge(
            transportType: _controller.activeTransport,
            status: _controller.isConnected
                ? ConnectionStatus.streaming
                : (_controller.isConnecting ? ConnectionStatus.connecting : ConnectionStatus.disconnected),
          ),
          const SizedBox(height: 16),
          if (_controller.isConnected) ...[
            Text(
              'Connected to: ${_controller.connectedDeviceIp}',
              style: const TextStyle(color: VeyraColors.textPrimary, fontWeight: FontWeight.w600, fontSize: 13),
            ),
            const SizedBox(height: 8),
            Text(
              'Profile: ${_controller.profile.name}',
              style: const TextStyle(color: VeyraColors.textSecondary, fontSize: 12),
            ),
            const Spacer(),
            ElevatedButton.icon(
              onPressed: _controller.disconnectDevice,
              icon: const Icon(Icons.link_off),
              label: const Text('DISCONNECT'),
              style: ElevatedButton.styleFrom(
                backgroundColor: VeyraColors.error,
                foregroundColor: Colors.white,
                minimumSize: const Size.fromHeight(42),
              ),
            ),
          ] else ...[
            const Text(
              'No phone connected yet. Click below to establish connection over Wi-Fi, USB, or Bluetooth.',
              style: TextStyle(color: VeyraColors.textSecondary, fontSize: 12),
            ),
            const Spacer(),
            ElevatedButton.icon(
              onPressed: () {
                showDialog(
                  context: context,
                  builder: (ctx) => DeviceDiscoveryDialog(
                    onConnect: (ip, port) {
                      _controller.connectDevice(ip, port);
                    },
                  ),
                );
              },
              icon: const Icon(Icons.add_link),
              label: const Text('CONNECT PHONE'),
              style: ElevatedButton.styleFrom(
                backgroundColor: VeyraColors.primary,
                foregroundColor: Colors.black,
                minimumSize: const Size.fromHeight(42),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _buildCenterPreview() {
    return Container(
      color: Colors.black,
      child: Center(
        child: _controller.isConnected
            ? AspectRatio(
                aspectRatio: 16 / 9,
                child: Container(
                  decoration: BoxDecoration(
                    color: VeyraColors.surfaceElevated,
                    border: Border.all(color: VeyraColors.border),
                  ),
                  child: const Center(
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(Icons.videocam, size: 64, color: VeyraColors.primary),
                        SizedBox(height: 12),
                        Text(
                          'Zero-Copy Direct3D 11 NV12 Texture Feed Active',
                          style: TextStyle(color: VeyraColors.textPrimary, fontWeight: FontWeight.bold),
                        ),
                      ],
                    ),
                  ),
                ),
              )
            : Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.videocam_off, size: 64, color: VeyraColors.textMuted.withAlpha(128)),
                  const SizedBox(height: 16),
                  const Text(
                    'No Active Camera Feed',
                    style: TextStyle(color: VeyraColors.textSecondary, fontSize: 16),
                  ),
                ],
              ),
      ),
    );
  }

  Widget _buildRightControls() {
    return Container(
      width: 320,
      decoration: const BoxDecoration(
        color: VeyraColors.surface,
        border: Border(left: BorderSide(color: VeyraColors.border)),
      ),
      padding: const EdgeInsets.all(16),
      child: ListView(
        children: [
          const Text(
            'REMOTE CAMERA CONTROLS',
            style: TextStyle(
              color: VeyraColors.textSecondary,
              fontSize: 11,
              fontWeight: FontWeight.bold,
              letterSpacing: 1.0,
            ),
          ),
          const SizedBox(height: 16),

          // Zoom
          const Text('DIGITAL ZOOM', style: TextStyle(color: VeyraColors.textSecondary, fontSize: 10, fontWeight: FontWeight.bold)),
          ZoomSlider(
            zoom: _controller.controls.zoom,
            maxZoom: 8.0,
            onChanged: _controller.isConnected ? (val) => _controller.setZoom(val) : (_) {},
          ),
          const SizedBox(height: 16),

          // Exposure
          const Text('EXPOSURE COMPENSATION', style: TextStyle(color: VeyraColors.textSecondary, fontSize: 10, fontWeight: FontWeight.bold)),
          ExposureSlider(
            exposure: _controller.controls.exposure,
            onChanged: _controller.isConnected ? (val) => _controller.setExposure(val) : (_) {},
          ),
          const SizedBox(height: 16),

          // Focus
          const Text('FOCUS MODE', style: TextStyle(color: VeyraColors.textSecondary, fontSize: 10, fontWeight: FontWeight.bold)),
          ManualFocusControl(
            autoFocus: _controller.controls.autoFocus,
            distance: _controller.controls.manualFocusDistance,
            onToggleAutoFocus: _controller.isConnected ? (val) => _controller.setFocus(autoFocus: val) : (_) {},
            onDistanceChanged: _controller.isConnected ? (val) => _controller.setFocus(autoFocus: false, distance: val) : (_) {},
          ),
          const SizedBox(height: 24),

          // Quick Action Buttons
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: _controller.isConnected ? () => _controller.setTorch(!_controller.controls.torch) : null,
                  icon: Icon(_controller.controls.torch ? Icons.flash_on : Icons.flash_off, size: 16),
                  label: Text(_controller.controls.torch ? 'FLASH ON' : 'FLASH OFF'),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: _controller.isConnected ? _controller.switchCamera : null,
                  icon: const Icon(Icons.flip_camera_android, size: 16),
                  label: const Text('FLIP CAM'),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          OutlinedButton.icon(
            onPressed: _controller.isConnected ? _controller.requestIdr : null,
            icon: const Icon(Icons.refresh, size: 16),
            label: const Text('FORCE KEYFRAME (IDR)'),
            style: OutlinedButton.styleFrom(minimumSize: const Size.fromHeight(36)),
          ),
        ],
      ),
    );
  }
}
