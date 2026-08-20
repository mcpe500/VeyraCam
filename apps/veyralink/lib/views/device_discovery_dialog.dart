import 'package:flutter/material.dart';
import 'package:veyra_ui/veyra_ui.dart';

class DeviceDiscoveryDialog extends StatefulWidget {
  final Function(String ip, int port, String pin) onConnect;

  const DeviceDiscoveryDialog({super.key, required this.onConnect});

  @override
  State<DeviceDiscoveryDialog> createState() => _DeviceDiscoveryDialogState();
}

class _DeviceDiscoveryDialogState extends State<DeviceDiscoveryDialog> {
  final TextEditingController _ipController = TextEditingController(text: '192.168.1.');
  final TextEditingController _portController = TextEditingController(text: '5150');
  final TextEditingController _pinController = TextEditingController();

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      backgroundColor: VeyraColors.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: VeyraColors.border),
      ),
      title: const Row(
        children: [
          Icon(Icons.phonelink, color: VeyraColors.primary),
          SizedBox(width: 10),
          Text('Connect to Phone', style: TextStyle(color: VeyraColors.textPrimary)),
        ],
      ),
      content: SizedBox(
        width: 380,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Enter the IP address and 6-digit pairing PIN shown on your phone:',
              style: TextStyle(color: VeyraColors.textSecondary, fontSize: 13),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _ipController,
              decoration: const InputDecoration(
                labelText: 'Phone IP Address',
                hintText: 'e.g. 192.168.1.105',
                prefixIcon: Icon(Icons.wifi, color: VeyraColors.primary),
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _portController,
              keyboardType: TextInputType.number,
              decoration: const InputDecoration(
                labelText: 'Control Port',
                hintText: '5150',
                prefixIcon: Icon(Icons.settings_ethernet, color: VeyraColors.primary),
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _pinController,
              keyboardType: TextInputType.number,
              maxLength: 6,
              obscureText: true,
              decoration: const InputDecoration(
                labelText: 'Pairing PIN',
                hintText: '6-digit PIN from phone screen',
                prefixIcon: Icon(Icons.pin, color: VeyraColors.primary),
                border: OutlineInputBorder(),
                counterText: '',
              ),
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel', style: TextStyle(color: VeyraColors.textSecondary)),
        ),
        ElevatedButton(
          onPressed: () {
            final ip = _ipController.text.trim();
            final port = int.tryParse(_portController.text.trim()) ?? 5150;
            final pin = _pinController.text.trim();
            if (ip.isNotEmpty && pin.length == 6) {
              Navigator.pop(context);
              widget.onConnect(ip, port, pin);
            } else {
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Enter a valid IP address and 6-digit PIN'),
                ),
              );
            }
          },
          style: ElevatedButton.styleFrom(
            backgroundColor: VeyraColors.primary,
            foregroundColor: Colors.black,
          ),
          child: const Text('Connect', style: TextStyle(fontWeight: FontWeight.bold)),
        ),
      ],
    );
  }
}
