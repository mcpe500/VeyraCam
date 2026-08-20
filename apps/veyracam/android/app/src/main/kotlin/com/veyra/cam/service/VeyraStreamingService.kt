package com.veyra.cam.service

import android.annotation.SuppressLint
import android.app.*
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.SurfaceTexture
import android.net.wifi.WifiManager
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.util.Log
import androidx.core.app.NotificationCompat
import com.veyra.cam.audio.AudioEngine
import com.veyra.cam.camera.Camera2Controller
import com.veyra.cam.camera.CameraCapabilities
import com.veyra.cam.codec.H264Encoder
import com.veyra.cam.thermal.ThermalController
import com.veyra.cam.thermal.ThermalRecommendation
import com.veyra.cam.transport.BluetoothTransport
import com.veyra.cam.transport.UdpTransport
import com.veyra.cam.transport.UsbTransport
import org.json.JSONObject

class VeyraStreamingService : Service() {
    companion object {
        private const val TAG = "VeyraStreamingService"
        private const val NOTIFICATION_CHANNEL_ID = "veyra_streaming_channel"
        private const val NOTIFICATION_ID = 5150
        const val ACTION_STOP_STREAMING = "com.veyra.cam.STOP_STREAMING"
        const val ACTION_START_STREAMING = "com.veyra.cam.START_STREAMING"

        var instance: VeyraStreamingService? = null
            private set
    }

    inner class LocalBinder : Binder() {
        fun getService(): VeyraStreamingService = this@VeyraStreamingService
    }

    private val binder = LocalBinder()

    private var nativeHandle: Long = 0
    private var cameraController: Camera2Controller? = null
    private var encoder: H264Encoder? = null
    private var audioEngine: AudioEngine? = null
    private var thermalController: ThermalController? = null
    private var udpTransport: UdpTransport? = null
    private var bluetoothTransport: BluetoothTransport? = null
    private var usbTransport: UsbTransport? = null

    private var wakeLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null

    private var isStreaming = false
    private var previewTexture: SurfaceTexture? = null
    private var isBackCamera = true
    private var currentWidth = 1280
    private var currentHeight = 720
    private var currentFps = 30
    private var currentBitrateBps = 2500000

    private val screenStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Intent.ACTION_SCREEN_OFF -> {
                    Log.i(TAG, "Screen off detected — entering low-power Screen-Off Mode (disabling preview surface)")
                    cameraController?.updatePreviewSurface(null)
                }
                Intent.ACTION_SCREEN_ON -> {
                    Log.i(TAG, "Screen on detected — restoring preview surface if available")
                    if (previewTexture != null) {
                        cameraController?.updatePreviewSurface(previewTexture)
                    }
                }
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        createNotificationChannel()

        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_SCREEN_OFF)
            addAction(Intent.ACTION_SCREEN_ON)
        }
        registerReceiver(screenStateReceiver, filter)

        // Acquire WakeLock & WifiLock
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Veyra::StreamingWakeLock")
        wakeLock?.acquire(12 * 60 * 60 * 1000L) // 12 hours max

        val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        wifiLock = wifiManager.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "Veyra::WifiLock")
        wifiLock?.acquire()

        Log.i(TAG, "VeyraStreamingService created")
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP_STREAMING) {
            stopStreaming()
            stopForeground(true)
            stopSelf()
            return START_NOT_STICKY
        }

        startForeground(NOTIFICATION_ID, buildNotification("Ready to stream"))
        return START_STICKY
    }

    @SuppressLint("MissingPermission")
    fun startStreaming(
        surfaceTexture: SurfaceTexture?,
        facingBack: Boolean = true,
        width: Int = 1280,
        height: Int = 720,
        fps: Int = 30,
        bitrateBps: Int = 2500000,
        onStarted: (() -> Unit)? = null
    ) {
        if (isStreaming) return
        isStreaming = true
        previewTexture = surfaceTexture
        isBackCamera = facingBack
        currentWidth = width
        currentHeight = height
        currentFps = fps
        currentBitrateBps = bitrateBps

        // 1. Create native core session
        val sessionId = (System.currentTimeMillis() and 0x7FFFFFFF).toInt()
        nativeHandle = VeyraNativeBridge.nativeCreateSession(sessionId)

        // 2. Initialize thermal management
        thermalController = ThermalController(this) { recommendation ->
            onThermalAdjustment(recommendation)
        }.apply { start() }

        // 3. Initialize H.264 Encoder
        encoder = H264Encoder(
            width = currentWidth,
            height = currentHeight,
            fps = currentFps,
            bitrateBps = currentBitrateBps,
            nativeHandleProvider = { nativeHandle }
        )
        val codecSurface = encoder!!.start()

        // 4. Initialize Camera2 Controller
        cameraController = Camera2Controller(this).apply {
            openCamera(
                facingBack = isBackCamera,
                targetWidth = currentWidth,
                targetHeight = currentHeight,
                fps = currentFps,
                codecSurface = codecSurface,
                previewSurfaceTexture = previewTexture,
                onOpened = {
                    Log.i(TAG, "Camera pipeline ready and streaming")
                    onStarted?.invoke()
                }
            )
        }

        // 5. Initialize Audio Engine
        audioEngine = AudioEngine(
            sampleRate = 48000,
            channelCount = 1,
            bitrateBps = 32000,
            nativeHandleProvider = { nativeHandle }
        ).apply { start() }

        // 6. Initialize Network & Transports
        udpTransport = UdpTransport(
            controlPort = 5150,
            onControlCommandReceived = { opCode, payload ->
                handleControlCommand(opCode, payload)
            },
            onClientConnected = { clientIp, udpPort ->
                Log.i(TAG, "Client connected: $clientIp:$udpPort")
                VeyraNativeBridge.nativeConfigureUdpDestination(nativeHandle, clientIp, udpPort)
                updateNotification("Streaming to $clientIp")
            },
            onClientDisconnected = {
                Log.i(TAG, "Client disconnected")
                updateNotification("Waiting for connection...")
            }
        ).apply { start() }

        bluetoothTransport = BluetoothTransport(
            onDataReceived = { data, len -> },
            onConnected = { Log.i(TAG, "Bluetooth transport active") },
            onDisconnected = { Log.i(TAG, "Bluetooth transport disconnected") }
        ).apply { start() }

        usbTransport = UsbTransport(this) { connected ->
            Log.i(TAG, "USB status changed: $connected")
        }.apply { start() }

        updateNotification("Streaming active ($currentWidth x $currentHeight @ ${currentFps}fps)")
    }

    private fun handleControlCommand(opCode: Int, payload: JSONObject) {
        when (opCode) {
            0x01 -> { // HELLO
                val caps = CameraCapabilities(this).toCapabilitiesJson(Build.MODEL, "android-${Build.SERIAL ?: "dev"}")
                udpTransport?.sendControlMessage(caps)
            }
            0x03 -> { // START_STREAM
                val video = payload.optJSONObject("video")
                if (video != null) {
                    val w = video.optInt("width", currentWidth)
                    val h = video.optInt("height", currentHeight)
                    val fps = video.optInt("fps", currentFps)
                    val br = video.optInt("bitrate", currentBitrateBps)
                    if (br != currentBitrateBps) {
                        encoder?.updateBitrate(br)
                        currentBitrateBps = br
                    }
                }
            }
            0x05 -> { // SET_ZOOM
                val zoom = payload.optDouble("zoom", 1.0).toFloat()
                cameraController?.setZoom(zoom)
            }
            0x06 -> { // SET_EXPOSURE
                val exp = payload.optInt("exposure", 0)
                cameraController?.setExposureCompensation(exp)
            }
            0x07 -> { // SET_FOCUS
                val auto = payload.optBoolean("autoFocus", true)
                val dist = payload.optDouble("distance", 0.0).toFloat()
                cameraController?.setFocus(auto, dist)
            }
            0x08 -> { // REQUEST_IDR
                encoder?.requestIdr()
            }
            0x09 -> { // SET_BITRATE
                val br = payload.optInt("bitrate", currentBitrateBps)
                encoder?.updateBitrate(br)
            }
        }
    }

    private fun onThermalAdjustment(rec: ThermalRecommendation) {
        Log.w(TAG, "Applying thermal recommendation: ${rec.tier}, ${rec.targetBitrateBps} bps")
        encoder?.updateBitrate(rec.targetBitrateBps)
    }

    fun setZoom(factor: Float) = cameraController?.setZoom(factor)
    fun setExposure(step: Int) = cameraController?.setExposureCompensation(step)
    fun setFocus(autoFocus: Boolean, distance: Float) = cameraController?.setFocus(autoFocus, distance)
    fun setTorch(enabled: Boolean) = cameraController?.setTorch(enabled)
    fun requestIdr() = encoder?.requestIdr()

    fun switchCamera(onComplete: (() -> Unit)? = null) {
        isBackCamera = !isBackCamera
        val enc = encoder ?: return
        cameraController?.closeCamera()
        val codecSurface = enc.start()
        cameraController = Camera2Controller(this).apply {
            openCamera(
                facingBack = isBackCamera,
                targetWidth = currentWidth,
                targetHeight = currentHeight,
                fps = currentFps,
                codecSurface = codecSurface,
                previewSurfaceTexture = previewTexture,
                onOpened = onComplete
            )
        }
    }

    fun getTelemetryJson(): String {
        return if (nativeHandle != 0L) {
            VeyraNativeBridge.nativeGetTelemetryJson(nativeHandle)
        } else {
            "{}"
        }
    }

    fun stopStreaming() {
        if (!isStreaming) return
        isStreaming = false

        cameraController?.closeCamera()
        cameraController = null

        encoder?.stop()
        encoder = null

        audioEngine?.stop()
        audioEngine = null

        thermalController?.stop()
        thermalController = null

        udpTransport?.stop()
        udpTransport = null

        bluetoothTransport?.stop()
        bluetoothTransport = null

        usbTransport?.stop()
        usbTransport = null

        if (nativeHandle != 0L) {
            VeyraNativeBridge.nativeDestroySession(nativeHandle)
            nativeHandle = 0L
        }

        Log.i(TAG, "Streaming stopped completely")
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                "Veyra Streaming Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Shows status of active camera streaming"
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(status: String): Notification {
        val stopIntent = Intent(this, VeyraStreamingService::class.java).apply {
            action = ACTION_STOP_STREAMING
        }
        val stopPendingIntent = PendingIntent.getService(
            this, 0, stopIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) PendingIntent.FLAG_IMMUTABLE else 0)
        )

        return NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setContentTitle("VeyraCam")
            .setContentText(status)
            .setSmallIcon(android.R.drawable.ic_menu_camera)
            .setOngoing(true)
            .addAction(android.R.drawable.ic_menu_close_clear_cancel, "Stop", stopPendingIntent)
            .build()
    }

    private fun updateNotification(status: String) {
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        manager.notify(NOTIFICATION_ID, buildNotification(status))
    }

    override fun onDestroy() {
        stopStreaming()
        try {
            unregisterReceiver(screenStateReceiver)
        } catch (_: Exception) {}

        if (wakeLock?.isHeld == true) wakeLock?.release()
        if (wifiLock?.isHeld == true) wifiLock?.release()

        instance = null
        super.onDestroy()
        Log.i(TAG, "VeyraStreamingService destroyed")
    }
}
