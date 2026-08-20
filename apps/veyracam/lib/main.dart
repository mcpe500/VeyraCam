import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:veyra_ui/veyra_ui.dart';
import 'views/camera_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  SystemChrome.setPreferredOrientations([
    DeviceOrientation.portraitUp,
    DeviceOrientation.portraitDown,
  ]);
  runApp(const VeyraCamApp());
}

class VeyraCamApp extends StatelessWidget {
  const VeyraCamApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'VeyraCam',
      theme: VeyraTheme.darkTheme,
      debugShowCheckedModeBanner: false,
      home: const CameraScreen(),
    );
  }
}
