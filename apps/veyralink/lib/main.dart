import 'package:flutter/material.dart';
import 'package:veyra_ui/veyra_ui.dart';
import 'views/desktop_main_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const VeyraLinkApp());
}

class VeyraLinkApp extends StatelessWidget {
  const VeyraLinkApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'VeyraLink',
      theme: VeyraTheme.darkTheme,
      debugShowCheckedModeBanner: false,
      home: const DesktopMainScreen(),
    );
  }
}
