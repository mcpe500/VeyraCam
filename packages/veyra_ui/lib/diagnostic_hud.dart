import 'package:flutter/material.dart';
import 'package:veyra_models/veyra_models.dart';
import 'theme.dart';

class DiagnosticHud extends StatelessWidget {
  final TelemetryStats stats;
  final bool isExpanded;
  final VoidCallback? onToggleExpand;

  const DiagnosticHud({
    super.key,
    required this.stats,
    this.isExpanded = false,
    this.onToggleExpand,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: VeyraColors.surface.withAlpha(235),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: VeyraColors.border, width: 1),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withAlpha(100),
            blurRadius: 10,
            offset: const Offset(0, 4),
          ),
        ],
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Header summary row
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              _metricChip('FPS', stats.fps.toStringAsFixed(1), VeyraColors.primary),
              const SizedBox(width: 8),
              _metricChip('Bitrate', stats.formattedBitrate, VeyraColors.textPrimary),
              const SizedBox(width: 8),
              _metricChip(
                'Latency',
                '${stats.latencyMs.toStringAsFixed(0)} ms',
                stats.latencyMs < 80 ? VeyraColors.success : VeyraColors.warning,
              ),
              const SizedBox(width: 8),
              _metricChip(
                'Loss',
                '${(stats.packetLossPercent * 100).toStringAsFixed(1)}%',
                stats.packetLossPercent < 0.02 ? VeyraColors.success : VeyraColors.error,
              ),
              if (onToggleExpand != null) ...[
                const SizedBox(width: 8),
                InkWell(
                  onTap: onToggleExpand,
                  child: Icon(
                    isExpanded ? Icons.keyboard_arrow_up : Icons.keyboard_arrow_down,
                    color: VeyraColors.textSecondary,
                    size: 18,
                  ),
                ),
              ],
            ],
          ),

          if (isExpanded) ...[
            const Divider(color: VeyraColors.border, height: 16),
            const Text(
              'LATENCY WATERFALL BREAKDOWN',
              style: TextStyle(
                color: VeyraColors.textSecondary,
                fontSize: 10,
                fontWeight: FontWeight.bold,
                letterSpacing: 1.0,
              ),
            ),
            const SizedBox(height: 8),
            _stageBar('Capture → Encode', stats.latencyBreakdown.captureToEncodeMs, 20.0),
            _stageBar('H.264 HW Encode', stats.latencyBreakdown.encodeDurationMs, 20.0),
            _stageBar('Network Transit', stats.latencyBreakdown.networkTransitMs, 40.0),
            _stageBar('Jitter Buffer', stats.latencyBreakdown.jitterBufferDelayMs, 30.0),
            _stageBar('MF HW Decode', stats.latencyBreakdown.decodeDurationMs, 20.0),
            _stageBar('D3D11 Present', stats.latencyBreakdown.presentationDelayMs, 10.0),
            const SizedBox(height: 6),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  'Temp: ${stats.temperatureCelsius}°C  |  Battery: ${stats.batteryPercent}%',
                  style: const TextStyle(color: VeyraColors.textSecondary, fontSize: 11),
                ),
                Text(
                  'Total: ${stats.latencyBreakdown.endToEndLatencyMs.toStringAsFixed(1)} ms',
                  style: const TextStyle(
                    color: VeyraColors.primary,
                    fontWeight: FontWeight.bold,
                    fontSize: 12,
                  ),
                ),
              ],
            ),
          ],
        ],
      ),
    );
  }

  Widget _metricChip(String label, String value, Color valueColor) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          label,
          style: const TextStyle(color: VeyraColors.textSecondary, fontSize: 9, fontWeight: FontWeight.w600),
        ),
        Text(
          value,
          style: TextStyle(color: valueColor, fontSize: 13, fontWeight: FontWeight.bold),
        ),
      ],
    );
  }

  Widget _stageBar(String label, double valueMs, double maxReferenceMs) {
    final progress = (valueMs / maxReferenceMs).clamp(0.02, 1.0);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        children: [
          SizedBox(
            width: 110,
            child: Text(
              label,
              style: const TextStyle(color: VeyraColors.textPrimary, fontSize: 11),
            ),
          ),
          Expanded(
            child: ClipRRect(
              borderRadius: BorderRadius.circular(3),
              child: LinearProgressIndicator(
                value: progress,
                backgroundColor: VeyraColors.surfaceElevated,
                valueColor: AlwaysStoppedAnimation<Color>(
                  valueMs > 30 ? VeyraColors.warning : VeyraColors.primary,
                ),
                minHeight: 6,
              ),
            ),
          ),
          const SizedBox(width: 8),
          SizedBox(
            width: 50,
            child: Text(
              '${valueMs.toStringAsFixed(1)} ms',
              textAlign: TextAlign.right,
              style: const TextStyle(color: VeyraColors.textSecondary, fontSize: 11, fontFamily: 'monospace'),
            ),
          ),
        ],
      ),
    );
  }
}
