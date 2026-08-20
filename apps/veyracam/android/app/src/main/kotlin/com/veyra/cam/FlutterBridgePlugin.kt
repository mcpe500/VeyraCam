package com.veyra.cam

import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.Looper
import androidx.annotation.NonNull
import com.veyra.cam.camera.CameraCapabilities
import com.veyra.cam.service.VeyraStreamingService
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.view.TextureRegistry

class FlutterBridgePlugin : FlutterPlugin, MethodChannel.MethodCallHandler, EventChannel.StreamHandler, ActivityAware {
    companion object {
        private const val METHOD_CHANNEL = "com.veyra.cam/control"
        private const val EVENT_CHANNEL = "com.veyra.cam/telemetry"
    }

    private lateinit var channel: MethodChannel
    private lateinit var eventChannel: EventChannel
    private lateinit var context: Context
    private var textureRegistry: TextureRegistry? = null
    private var surfaceEntry: TextureRegistry.SurfaceTextureEntry? = null

    private var eventSink: EventChannel.EventSink? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private var telemetryRunnable: Runnable? = null

    override fun onAttachedToEngine(@NonNull flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
        context = flutterPluginBinding.applicationContext
        textureRegistry = flutterPluginBinding.textureRegistry
        channel = MethodChannel(flutterPluginBinding.binaryMessenger, METHOD_CHANNEL)
        channel.setMethodCallHandler(this)

        eventChannel = EventChannel(flutterPluginBinding.binaryMessenger, EVENT_CHANNEL)
        eventChannel.setStreamHandler(this)
    }

    override fun onMethodCall(@NonNull call: MethodCall, @NonNull result: MethodChannel.Result) {
        when (call.method) {
            "getCapabilities" -> {
                val caps = CameraCapabilities(context).toCapabilitiesJson("Android Device", "android-dev")
                result.success(caps.toString())
            }
            "startStreaming" -> {
                val facingBack = call.argument<Boolean>("facingBack") ?: true
                val width = call.argument<Int>("width") ?: 1280
                val height = call.argument<Int>("height") ?: 720
                val fps = call.argument<Int>("fps") ?: 30
                val bitrate = call.argument<Int>("bitrate") ?: 2500000

                try {
                    // Create Texture Entry for zero-copy local preview in Flutter
                    surfaceEntry?.release()
                    val entry = try {
                        textureRegistry?.createSurfaceTexture()
                    } catch (_: Exception) {
                        @Suppress("DEPRECATION")
                        textureRegistry?.registerSurfaceTexture(android.graphics.SurfaceTexture(0))
                    }
                    surfaceEntry = entry
                    if (entry == null) {
                        result.error("TEXTURE_FAILED", "Failed to create Flutter surface texture", null)
                        return
                    }

                    // Start Foreground Service
                    val serviceIntent = Intent(context, VeyraStreamingService::class.java).apply {
                        action = VeyraStreamingService.ACTION_START_STREAMING
                    }
                    try {
                        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                            context.startForegroundService(serviceIntent)
                        } else {
                            context.startService(serviceIntent)
                        }
                    } catch (e: Exception) {
                        result.error("SERVICE_START_FAILED", e.message, null)
                        return
                    }

                    // Trigger streaming with retry for service bind (up to 1s)
                    var attempts = 0
                    var resultReplied = false
                    fun tryStart() {
                        val svc = VeyraStreamingService.instance
                        if (svc != null) {
                            try {
                                svc.startStreaming(
                                    surfaceTexture = entry.surfaceTexture(),
                                    facingBack = facingBack,
                                    width = width,
                                    height = height,
                                    fps = fps,
                                    bitrateBps = bitrate,
                                    onStarted = {
                                        if (!resultReplied) {
                                            resultReplied = true
                                            result.success(mapOf("textureId" to entry.id()))
                                        }
                                    }
                                )
                                // If camera onOpened is delayed, give it 2s then reply anyway with textureId
                                mainHandler.postDelayed({
                                    if (!resultReplied) {
                                        resultReplied = true
                                        result.success(mapOf("textureId" to entry.id()))
                                    }
                                }, 1500)
                            } catch (e: SecurityException) {
                                resultReplied = true
                                result.error("PERMISSION_DENIED", e.message ?: "Camera/Mic permission denied", null)
                            } catch (e: Exception) {
                                resultReplied = true
                                result.error("STREAM_FAILED", e.message ?: "Failed to start streaming", null)
                            }
                        } else if (attempts < 5) {
                            attempts++
                            mainHandler.postDelayed({ tryStart() }, 200)
                        } else {
                            if (!resultReplied) {
                                resultReplied = true
                                result.error("SERVICE_NOT_READY", "VeyraStreamingService not ready after 1s", null)
                            }
                        }
                    }
                    tryStart()
                } catch (e: Exception) {
                    result.error("START_FAILED", e.message, null)
                }
            }
            "stopStreaming" -> {
                VeyraStreamingService.instance?.stopStreaming()
                surfaceEntry?.release()
                surfaceEntry = null
                result.success(true)
            }
            "setZoom" -> {
                val zoom = call.argument<Double>("zoom")?.toFloat() ?: 1.0f
                VeyraStreamingService.instance?.setZoom(zoom)
                result.success(true)
            }
            "setExposure" -> {
                val exposure = call.argument<Int>("exposure") ?: 0
                VeyraStreamingService.instance?.setExposure(exposure)
                result.success(true)
            }
            "setFocus" -> {
                val autoFocus = call.argument<Boolean>("autoFocus") ?: true
                val distance = call.argument<Double>("distance")?.toFloat() ?: 0.0f
                VeyraStreamingService.instance?.setFocus(autoFocus, distance)
                result.success(true)
            }
            "setTorch" -> {
                val enabled = call.argument<Boolean>("enabled") ?: false
                VeyraStreamingService.instance?.setTorch(enabled)
                result.success(true)
            }
            "switchCamera" -> {
                VeyraStreamingService.instance?.switchCamera {
                    result.success(true)
                }
            }
            "requestIdr" -> {
                VeyraStreamingService.instance?.requestIdr()
                result.success(true)
            }
            "getPairingPin" -> {
                val pin = VeyraStreamingService.instance?.getPairingPin()
                if (pin != null) {
                    result.success(pin)
                } else {
                    result.success(null)
                }
            }
            else -> result.notImplemented()
        }
    }

    override fun onListen(arguments: Any?, events: EventChannel.EventSink?) {
        eventSink = events
        telemetryRunnable = object : Runnable {
            override fun run() {
                val json = VeyraStreamingService.instance?.getTelemetryJson() ?: "{}"
                eventSink?.success(json)
                mainHandler.postDelayed(this, 300) // ~3.3 Hz UI update rate
            }
        }
        mainHandler.post(telemetryRunnable!!)
    }

    override fun onCancel(arguments: Any?) {
        telemetryRunnable?.let { mainHandler.removeCallbacks(it) }
        telemetryRunnable = null
        eventSink = null
    }

    override fun onDetachedFromEngine(@NonNull binding: FlutterPlugin.FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
        eventChannel.setStreamHandler(null)
        surfaceEntry?.release()
        surfaceEntry = null
    }

    override fun onAttachedToActivity(binding: ActivityPluginBinding) {}
    override fun onDetachedFromActivityForConfigChanges() {}
    override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {}
    override fun onDetachedFromActivity() {}
}
