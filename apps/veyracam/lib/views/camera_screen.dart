import 'package:flutter/material.dart';
import 'package:veyra_ui/veyra_ui.dart';
import '../controllers/camera_controller.dart';
import '../widgets/camera_overlay.dart';
import 'settings_screen.dart';

class CameraScreen extends StatefulWidget {
  const CameraScreen({super.key});

  @override
  State<CameraScreen> createState() => _CameraScreenState();
}

class _CameraScreenState extends State<CameraScreen> {
  final VeyraCamController _controller = VeyraCamController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: ListenableBuilder(
        listenable: _controller,
        builder: (context, _) {
          return Stack(
            fit: StackFit.expand,
            children: [
              // Hardware Texture Preview (Zero Dart Frame Copy)
              if (_controller.textureId != null)
                Center(
                  child: AspectRatio(
                    aspectRatio: _controller.profile.resolution.width /
                        _controller.profile.resolution.height,
                    child: Texture(textureId: _controller.textureId!),
                  ),
                )
              else
                _buildIdlePlaceholder(),

              // Floating Controls & Telemetry Overlay
              CameraOverlay(
                controller: _controller,
                onOpenSettings: () {
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (ctx) => SettingsScreen(controller: _controller),
                    ),
                  );
                },
              ),
            ],
          );
        },
      ),
    );
  }

  Widget _buildIdlePlaceholder() {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            padding: const EdgeInsets.all(24),
            decoration: BoxDecoration(
              color: VeyraColors.surfaceElevated,
              shape: BoxShape.circle,
              border: Border.all(color: VeyraColors.primary.withAlpha(100), width: 2),
            ),
            child: const Icon(
              Icons.videocam_outlined,
              size: 56,
              color: VeyraColors.primary,
            ),
          ),
          const SizedBox(height: 20),
          const Text(
            'VeyraCam Mobile',
            style: TextStyle(
              color: VeyraColors.textPrimary,
              fontSize: 22,
              fontWeight: FontWeight.bold,
              letterSpacing: 0.5,
            ),
          ),
          const SizedBox(height: 8),
          const Text(
            'High-Performance Zero-Copy Virtual Webcam',
            style: TextStyle(color: VeyraColors.textSecondary, fontSize: 13),
          ),
        ],
      ),
    );
  }
}
