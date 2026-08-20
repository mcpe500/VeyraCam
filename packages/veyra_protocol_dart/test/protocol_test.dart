import 'dart:convert';
import 'package:test/test.dart';
import 'package:veyra_models/veyra_models.dart';
import 'package:veyra_protocol_dart/veyra_protocol_dart.dart';

void main() {
  group('Veyra Protocol Tests', () {
    test('Protocol constants verification', () {
      expect(ProtocolConstants.magic, equals(0x56455952));
      expect(ProtocolConstants.version, equals(1));
      expect(ProtocolConstants.defaultControlPort, equals(5150));
      expect(ProtocolConstants.defaultVideoPort, equals(5151));
      expect(ProtocolConstants.maxPacketSize, equals(1200));
    });

    test('Handshake hello message JSON serialization', () {
      final helloJson = HandshakeMessage.createHello(
        deviceName: 'Pixel 7',
        deviceId: 'px-001',
        model: 'Pixel 7 Pro',
      );

      final map = jsonDecode(helloJson) as Map<String, dynamic>;
      expect(map['type'], equals('hello'));
      expect(map['protocol'], equals(1));
      expect(map['device']['name'], equals('Pixel 7'));
    });

    test('StartStream message JSON serialization', () {
      const config = StreamConfig(
        resolution: Resolution(1280, 720),
        fps: 30,
        bitrateBps: 2500000,
      );

      final msg = HandshakeMessage.createStartStream(config: config);
      final map = jsonDecode(msg) as Map<String, dynamic>;
      expect(map['type'], equals('startStream'));
      expect(map['video']['width'], equals(1280));
      expect(map['video']['height'], equals(720));
      expect(map['video']['fps'], equals(30));
    });

    test('Control commands JSON serialization', () {
      final zoomMsg = ControlCommands.setZoom(2.5);
      expect(jsonDecode(zoomMsg)['zoom'], equals(2.5));

      final expMsg = ControlCommands.setExposure(-2);
      expect(jsonDecode(expMsg)['exposure'], equals(-2));

      final idrMsg = ControlCommands.requestIdr();
      expect(jsonDecode(idrMsg)['type'], equals('requestIdr'));
    });
  });
}
