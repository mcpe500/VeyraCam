import 'package:meta/meta.dart';

enum TransportType {
  auto,
  usb,
  wifiLan,
  wifiDirect,
  bluetooth
}

enum ConnectionStatus {
  disconnected,
  discovering,
  pairing,
  connecting,
  connected,
  streaming,
  reconnecting,
  error
}

@immutable
class LinkQuality {
  final TransportType activeTransport;
  final double rttMs;
  final double packetLossRate;
  final double jitterMs;
  final int bandwidthEstimateBps;

  const LinkQuality({
    required this.activeTransport,
    this.rttMs = 0.0,
    this.packetLossRate = 0.0,
    this.jitterMs = 0.0,
    this.bandwidthEstimateBps = 0,
  });

  bool get isHealthy => packetLossRate < 0.05 && rttMs < 100.0;
}
