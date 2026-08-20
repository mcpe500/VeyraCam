import 'dart:io';
import 'package:test/test.dart';
import 'package:veyra_native/veyra_native.dart';

void main() {
  group('Veyra Native FFI Tests', () {
    String resolveLibPath() {
      final candidates = [
        '../../native/windows/build/libveyra_core_api.so',
        './native/windows/build/libveyra_core_api.so',
        '/mnt/data/projects/VeyraCam/native/windows/build/libveyra_core_api.so',
      ];
      for (final p in candidates) {
        if (File(p).existsSync()) return p;
      }
      return 'libveyra_core_api.so';
    }

    test('Bindings instantiation and library symbol lookup', () {
      final libPath = resolveLibPath();
      final bindings = VeyraNativeBindings(libPath);
      expect(bindings, isNotNull);

      final initRes = bindings.veyraCoreInit();
      expect(initRes, equals(0));

      final ptr = bindings.veyraCoreGetTelemetryJson();
      expect(ptr.address, isNot(equals(0)));
      bindings.veyraCoreFreeString(ptr);

      bindings.veyraCoreShutdown();
    });

    test('High-level NativeDesktopSession lifecycle', () {
      final libPath = resolveLibPath();
      final session = NativeDesktopSession(
        VeyraNativeBindings(libPath),
      );

      final initOk = session.initialize();
      expect(initOk, isTrue);

      final handle = session.getSharedTextureHandle();
      expect(handle, isNotNull);

      session.shutdown();
      expect(session.isConnected, isFalse);
    });
  });
}
