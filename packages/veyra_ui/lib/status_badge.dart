import 'package:flutter/material.dart';
import 'package:veyra_models/veyra_models.dart';
import 'theme.dart';

class StatusBadge extends StatelessWidget {
  final TransportType transportType;
  final ConnectionStatus status;

  const StatusBadge({
    super.key,
    required this.transportType,
    required this.status,
  });

  @override
  Widget build(BuildContext context) {
    final (label, icon, color) = _getProperties();

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
      decoration: BoxDecoration(
        color: color.withAlpha(30),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withAlpha(100), width: 1),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 14, color: color),
          const SizedBox(width: 6),
          Text(
            label,
            style: TextStyle(
              color: color,
              fontSize: 12,
              fontWeight: FontWeight.w600,
              letterSpacing: 0.3,
            ),
          ),
        ],
      ),
    );
  }

  (String, IconData, Color) _getProperties() {
    switch (status) {
      case ConnectionStatus.streaming:
        switch (transportType) {
          case TransportType.usb:
            return ('USB 3.0 / ADB', Icons.usb, VeyraColors.success);
          case TransportType.wifiLan:
            return ('Wi-Fi LAN (5 GHz)', Icons.wifi, VeyraColors.primary);
          case TransportType.wifiDirect:
            return ('Wi-Fi Direct', Icons.wifi_tethering, VeyraColors.accent);
          case TransportType.bluetooth:
            return ('Bluetooth Classic', Icons.bluetooth, VeyraColors.warning);
          default:
            return ('Connected', Icons.check_circle, VeyraColors.success);
        }
      case ConnectionStatus.connecting:
        return ('Connecting...', Icons.sync, VeyraColors.warning);
      case ConnectionStatus.pairing:
        return ('Pairing...', Icons.phonelink_setup, VeyraColors.accent);
      case ConnectionStatus.disconnected:
      default:
        return ('Ready / Idle', Icons.power_settings_new, VeyraColors.textMuted);
    }
  }
}
