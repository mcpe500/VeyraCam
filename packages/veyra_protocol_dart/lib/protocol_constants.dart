class ProtocolConstants {
  static const int magic = 0x56455952; // 'VEYR'
  static const int version = 0x01;
  static const int defaultControlPort = 5150;
  static const int defaultVideoPort = 5151;
  static const int maxPacketSize = 1200;

  // Packet Flags
  static const int flagNone = 0x00;
  static const int flagKeyframe = 0x01;
  static const int flagAudio = 0x02;
  static const int flagEncrypted = 0x04;
  static const int flagTelemetry = 0x08;
  static const int flagDiscontinuity = 0x10;

  // Command OpCodes
  static const int opHello = 0x01;
  static const int opCapabilities = 0x02;
  static const int opStartStream = 0x03;
  static const int opStopStream = 0x04;
  static const int opSetZoom = 0x05;
  static const int opSetExposure = 0x06;
  static const int opSetFocus = 0x07;
  static const int opRequestIdr = 0x08;
  static const int opSetBitrate = 0x09;
  static const int opTransportSwitch = 0x0A;
  static const int opPing = 0x0B;
  static const int opPong = 0x0C;
  static const int opStats = 0x0D;
}
