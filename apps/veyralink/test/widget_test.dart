import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:veyralink/views/device_discovery_dialog.dart';

void main() {
  testWidgets('Device discovery dialog requires IP and 6-digit PIN',
      (WidgetTester tester) async {
    String? capturedIp;
    String? capturedPin;

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: Builder(
            builder: (context) => ElevatedButton(
              onPressed: () => showDialog(
                context: context,
                builder: (_) => DeviceDiscoveryDialog(
                  onConnect: (ip, port, pin) {
                    capturedIp = ip;
                    capturedPin = pin;
                  },
                ),
              ),
              child: const Text('open'),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();

    expect(find.text('Connect to Phone'), findsOneWidget);

    // Empty fields -> connect shows a SnackBar, nothing captured.
    await tester.tap(find.text('Connect'));
    await tester.pump();
    expect(capturedIp, isNull);

    // Fill IP + 6-digit PIN -> connect captures both.
    await tester.enterText(find.byType(TextField).at(0), '192.168.1.50');
    await tester.enterText(find.byType(TextField).at(2), '123456');
    await tester.tap(find.text('Connect'));
    await tester.pumpAndSettle();

    expect(capturedIp, '192.168.1.50');
    expect(capturedPin, '123456');
  });
}