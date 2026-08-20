package com.veyra.cam.thermal

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.PowerManager
import android.util.Log

enum class ThermalTier {
    NORMAL,    // <40°C: Full quality (e.g. 1080p30 @ 4.5 Mbps or 720p30 @ 2.5 Mbps)
    WARM,      // 40–44°C: 720p30 @ 2.0 Mbps (-20%)
    HOT,       // 45–48°C: 720p24 / 480p24 @ 1.2 Mbps (-50%)
    CRITICAL   // >48°C: Emergency 360p15 @ 350 kbps
}

data class ThermalRecommendation(
    val tier: ThermalTier,
    val targetWidth: Int,
    val targetHeight: Int,
    val targetFps: Int,
    val targetBitrateBps: Int,
    val temperatureCelsius: Float
)

class ThermalController(
    private val context: Context,
    private val onRecommendationChanged: (ThermalRecommendation) -> Unit
) {
    companion object {
        private const val TAG = "ThermalController"
    }

    private var currentTier = ThermalTier.NORMAL
    private var lastTempCelsius = 25.0f
    private var powerManager: PowerManager? = null
    private var thermalListener: PowerManager.OnThermalStatusChangedListener? = null

    private val batteryReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == Intent.ACTION_BATTERY_CHANGED) {
                val tempTenths = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0)
                val tempCelsius = tempTenths / 10.0f
                lastTempCelsius = tempCelsius
                evaluateThermalState(tempCelsius)
            }
        }
    }

    fun start() {
        // Register battery temperature receiver
        val filter = IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        context.registerReceiver(batteryReceiver, filter)

        // Register modern PowerManager thermal listener if API 29+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager
            thermalListener = PowerManager.OnThermalStatusChangedListener { status ->
                val tier = when (status) {
                    PowerManager.THERMAL_STATUS_NONE -> ThermalTier.NORMAL
                    PowerManager.THERMAL_STATUS_LIGHT -> ThermalTier.WARM
                    PowerManager.THERMAL_STATUS_MODERATE -> ThermalTier.HOT
                    PowerManager.THERMAL_STATUS_SEVERE,
                    PowerManager.THERMAL_STATUS_CRITICAL,
                    PowerManager.THERMAL_STATUS_EMERGENCY,
                    PowerManager.THERMAL_STATUS_SHUTDOWN -> ThermalTier.CRITICAL
                    else -> ThermalTier.NORMAL
                }
                applyTier(tier, lastTempCelsius)
            }
            thermalListener?.let { powerManager?.addThermalStatusListener(it) }
        }

        Log.i(TAG, "ThermalController started")
    }

    private fun evaluateThermalState(tempCelsius: Float) {
        val tier = when {
            tempCelsius >= 48.0f -> ThermalTier.CRITICAL
            tempCelsius >= 44.0f -> ThermalTier.HOT
            tempCelsius >= 40.0f -> ThermalTier.WARM
            else -> ThermalTier.NORMAL
        }
        applyTier(tier, tempCelsius)
    }

    private fun applyTier(tier: ThermalTier, tempCelsius: Float) {
        if (tier != currentTier) {
            currentTier = tier
            Log.w(TAG, "Thermal state transition: $tier (${tempCelsius}°C)")

            val recommendation = when (tier) {
                ThermalTier.NORMAL -> ThermalRecommendation(
                    tier = tier,
                    targetWidth = 1280,
                    targetHeight = 720,
                    targetFps = 30,
                    targetBitrateBps = 2500000,
                    temperatureCelsius = tempCelsius
                )
                ThermalTier.WARM -> ThermalRecommendation(
                    tier = tier,
                    targetWidth = 1280,
                    targetHeight = 720,
                    targetFps = 30,
                    targetBitrateBps = 1800000,
                    temperatureCelsius = tempCelsius
                )
                ThermalTier.HOT -> ThermalRecommendation(
                    tier = tier,
                    targetWidth = 854,
                    targetHeight = 480,
                    targetFps = 24,
                    targetBitrateBps = 1000000,
                    temperatureCelsius = tempCelsius
                )
                ThermalTier.CRITICAL -> ThermalRecommendation(
                    tier = tier,
                    targetWidth = 640,
                    targetHeight = 360,
                    targetFps = 15,
                    targetBitrateBps = 350000,
                    temperatureCelsius = tempCelsius
                )
            }
            onRecommendationChanged(recommendation)
        }
    }

    fun getCurrentTemperature(): Float = lastTempCelsius

    fun stop() {
        try {
            context.unregisterReceiver(batteryReceiver)
        } catch (_: Exception) {}

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && thermalListener != null) {
            powerManager?.removeThermalStatusListener(thermalListener!!)
            thermalListener = null
        }
        Log.i(TAG, "ThermalController stopped")
    }
}
