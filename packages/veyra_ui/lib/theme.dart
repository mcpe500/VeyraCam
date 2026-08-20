import 'package:flutter/material.dart';

class VeyraColors {
  static const Color background = Color(0xFF0D1117);
  static const Color surface = Color(0xFF161B22);
  static const Color surfaceElevated = Color(0xFF21262D);
  static const Color border = Color(0xFF30363D);

  static const Color primary = Color(0xFF00E5FF); // Neon Cyan
  static const Color primaryDark = Color(0xFF0097A7);
  static const Color accent = Color(0xFF7C4DFF); // Electric Purple

  static const Color success = Color(0xFF00E676); // Emerald Green
  static const Color warning = Color(0xFFFFD600); // Amber Yellow
  static const Color error = Color(0xFFFF1744);   // Coral Red

  static const Color textPrimary = Color(0xFFF0F6FC);
  static const Color textSecondary = Color(0xFF8B949E);
  static const Color textMuted = Color(0xFF484F58);
}

class VeyraTheme {
  static ThemeData get darkTheme {
    return ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      scaffoldBackgroundColor: VeyraColors.background,
      colorScheme: const ColorScheme.dark(
        primary: VeyraColors.primary,
        secondary: VeyraColors.accent,
        surface: VeyraColors.surface,
        error: VeyraColors.error,
      ),
      cardTheme: CardTheme(
        color: VeyraColors.surface,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: const BorderSide(color: VeyraColors.border, width: 1),
        ),
      ),
      appBarTheme: const AppBarTheme(
        backgroundColor: VeyraColors.surface,
        elevation: 0,
        centerTitle: true,
        titleTextStyle: TextStyle(
          color: VeyraColors.textPrimary,
          fontSize: 18,
          fontWeight: FontWeight.w600,
          letterSpacing: 0.5,
        ),
      ),
    );
  }
}
