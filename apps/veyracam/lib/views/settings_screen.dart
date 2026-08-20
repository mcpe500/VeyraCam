import 'package:flutter/material.dart';
import 'package:veyra_models/veyra_models.dart';
import 'package:veyra_ui/veyra_ui.dart';
import '../controllers/camera_controller.dart';

class SettingsScreen extends StatelessWidget {
  final VeyraCamController controller;

  const SettingsScreen({super.key, required this.controller});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Stream Settings'),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _sectionHeader('QUALITY PROFILES'),
          ...StreamProfile.presets.map((profile) {
            final isSelected = controller.profile.id == profile.id;
            return Card(
              color: isSelected ? VeyraColors.primary.withAlpha(25) : VeyraColors.surface,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
                side: BorderSide(
                  color: isSelected ? VeyraColors.primary : VeyraColors.border,
                  width: isSelected ? 1.5 : 1,
                ),
              ),
              child: ListTile(
                title: Text(
                  profile.name,
                  style: TextStyle(
                    color: isSelected ? VeyraColors.primary : VeyraColors.textPrimary,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                subtitle: Text(
                  '${profile.resolution.label} @ ${profile.fps}fps  •  ${(profile.bitrateBps / 1000000).toStringAsFixed(1)} Mbps',
                  style: const TextStyle(color: VeyraColors.textSecondary, fontSize: 12),
                ),
                trailing: isSelected
                    ? const Icon(Icons.check_circle, color: VeyraColors.primary)
                    : null,
                onTap: () {
                  controller.setProfile(profile);
                  Navigator.pop(context);
                },
              ),
            );
          }),

          const SizedBox(height: 16),
          _sectionHeader('DEVICE & POWER OPTIMIZATION'),
          Card(
            child: Column(
              children: [
                ListTile(
                  leading: const Icon(Icons.screen_lock_portrait, color: VeyraColors.primary),
                  title: const Text('Screen-Off Streaming Mode'),
                  subtitle: const Text(
                    'Automatically turns off preview when screen is locked to save battery and reduce heat.',
                    style: TextStyle(fontSize: 12, color: VeyraColors.textSecondary),
                  ),
                  trailing: const Icon(Icons.check, color: VeyraColors.success),
                ),
                const Divider(color: VeyraColors.border),
                ListTile(
                  leading: const Icon(Icons.thermostat, color: VeyraColors.warning),
                  title: const Text('Active Thermal Throttling'),
                  subtitle: const Text(
                    'Adapts bitrate and FPS dynamically before thermal limits are exceeded.',
                    style: TextStyle(fontSize: 12, color: VeyraColors.textSecondary),
                  ),
                  trailing: const Icon(Icons.check, color: VeyraColors.success),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _sectionHeader(String title) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 4),
      child: Text(
        title,
        style: const TextStyle(
          color: VeyraColors.textSecondary,
          fontSize: 11,
          fontWeight: FontWeight.bold,
          letterSpacing: 1.0,
        ),
      ),
    );
  }
}
