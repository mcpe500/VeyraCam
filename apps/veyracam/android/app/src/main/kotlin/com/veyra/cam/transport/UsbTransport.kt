package com.veyra.cam.transport

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbManager
import android.util.Log

class UsbTransport(
    private val context: Context,
    private val onUsbStateChanged: (connected: Boolean) -> Unit
) {
    companion object {
        private const val TAG = "UsbTransport"
    }

    private var isRegistered = false

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == "android.hardware.usb.action.USB_STATE") {
                val connected = intent.getBooleanExtra("connected", false)
                val configured = intent.getBooleanExtra("configured", false)
                Log.i(TAG, "USB State changed: connected=$connected, configured=$configured")
                onUsbStateChanged(connected)
            }
        }
    }

    fun start() {
        if (!isRegistered) {
            val filter = IntentFilter("android.hardware.usb.action.USB_STATE")
            context.registerReceiver(usbReceiver, filter)
            isRegistered = true
            Log.i(TAG, "UsbTransport monitoring started")
        }
    }

    fun stop() {
        if (isRegistered) {
            try {
                context.unregisterReceiver(usbReceiver)
            } catch (_: Exception) {}
            isRegistered = false
            Log.i(TAG, "UsbTransport monitoring stopped")
        }
    }
}
