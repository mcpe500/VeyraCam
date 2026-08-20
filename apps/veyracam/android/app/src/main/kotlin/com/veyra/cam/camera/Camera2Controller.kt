package com.veyra.cam.camera

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Rect
import android.graphics.SurfaceTexture
import android.hardware.camera2.*
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.util.Range
import android.util.Size
import android.view.Surface
import java.util.concurrent.Semaphore
import java.util.concurrent.TimeUnit

class Camera2Controller(private val context: Context) {
    companion object {
        private const val TAG = "Camera2Controller"
    }

    private val cameraManager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    private var cameraDevice: CameraDevice? = null
    private var captureSession: CameraCaptureSession? = null
    private var previewRequestBuilder: CaptureRequest.Builder? = null

    private var backgroundThread: HandlerThread? = null
    private var backgroundHandler: Handler? = null
    private val cameraOpenCloseLock = Semaphore(1)

    private var currentCameraId: String = "0"
    private var currentCharacteristics: CameraCharacteristics? = null
    private var encoderSurface: Surface? = null
    private var previewSurface: Surface? = null

    private var currentZoom: Float = 1.0f
    private var currentExposure: Int = 0
    private var isAutoFocus: Boolean = true
    private var manualFocusDistance: Float = 0.0f
    private var isTorchOn: Boolean = false
    private var isBackFacing: Boolean = true
    private var targetFps: Int = 30

    fun startBackgroundThread() {
        if (backgroundThread == null) {
            backgroundThread = HandlerThread("VeyraCamera2Background").apply { start() }
            backgroundHandler = Handler(backgroundThread!!.looper)
        }
    }

    fun stopBackgroundThread() {
        backgroundThread?.quitSafely()
        try {
            backgroundThread?.join()
            backgroundThread = null
            backgroundHandler = null
        } catch (e: InterruptedException) {
            Log.e(TAG, "Error stopping background thread", e)
        }
    }

    @SuppressLint("MissingPermission")
    fun openCamera(
        facingBack: Boolean = true,
        targetWidth: Int = 1280,
        targetHeight: Int = 720,
        fps: Int = 30,
        codecSurface: Surface,
        previewSurfaceTexture: SurfaceTexture? = null,
        onOpened: (() -> Unit)? = null
    ) {
        startBackgroundThread()
        isBackFacing = facingBack
        targetFps = fps
        encoderSurface = codecSurface

        if (previewSurfaceTexture != null) {
            previewSurfaceTexture.setDefaultBufferSize(
                if (targetWidth > 1280) 640 else targetWidth / 2,
                if (targetHeight > 720) 360 else targetHeight / 2
            )
            previewSurface = Surface(previewSurfaceTexture)
        } else {
            previewSurface = null
        }

        try {
            if (!cameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS)) {
                throw RuntimeException("Time out waiting to lock camera opening.")
            }

            val cameraIds = cameraManager.cameraIdList
            var selectedId = cameraIds.firstOrNull() ?: "0"
            for (id in cameraIds) {
                val chars = cameraManager.getCameraCharacteristics(id)
                val facing = chars.get(CameraCharacteristics.LENS_FACING)
                if (facingBack && facing == CameraCharacteristics.LENS_FACING_BACK) {
                    selectedId = id
                    break
                } else if (!facingBack && facing == CameraCharacteristics.LENS_FACING_FRONT) {
                    selectedId = id
                    break
                }
            }

            currentCameraId = selectedId
            currentCharacteristics = cameraManager.getCameraCharacteristics(currentCameraId)

            cameraManager.openCamera(currentCameraId, object : CameraDevice.StateCallback() {
                override fun onOpened(camera: CameraDevice) {
                    cameraOpenCloseLock.release()
                    cameraDevice = camera
                    createCaptureSession(onOpened)
                }

                override fun onDisconnected(camera: CameraDevice) {
                    cameraOpenCloseLock.release()
                    camera.close()
                    cameraDevice = null
                }

                override fun onError(camera: CameraDevice, error: Int) {
                    cameraOpenCloseLock.release()
                    camera.close()
                    cameraDevice = null
                    Log.e(TAG, "Camera device error: $error")
                }
            }, backgroundHandler)

        } catch (e: Exception) {
            Log.e(TAG, "Failed to open camera", e)
            cameraOpenCloseLock.release()
        }
    }

    private fun createCaptureSession(onReady: (() -> Unit)? = null) {
        val device = cameraDevice ?: return
        val encSurface = encoderSurface ?: return

        try {
            val surfaces = mutableListOf(encSurface)
            previewSurface?.let { surfaces.add(it) }

            device.createCaptureSession(surfaces, object : CameraCaptureSession.StateCallback() {
                override fun onConfigured(session: CameraCaptureSession) {
                    if (cameraDevice == null) return
                    captureSession = session
                    try {
                        val builder = device.createCaptureRequest(CameraDevice.TEMPLATE_RECORD)
                        builder.addTarget(encSurface)
                        previewSurface?.let { builder.addTarget(it) }

                        // Apply FPS range
                        val ranges = currentCharacteristics?.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)
                        val matchedRange = ranges?.firstOrNull { it.upper == targetFps } 
                            ?: ranges?.maxByOrNull { it.upper } 
                            ?: Range(15, 30)
                        builder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, matchedRange)

                        // Low latency video stabilization disabled to reduce delay
                        builder.set(CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE, CameraMetadata.CONTROL_VIDEO_STABILIZATION_MODE_OFF)
                        builder.set(CaptureRequest.LENS_OPTICAL_STABILIZATION_MODE, CameraMetadata.LENS_OPTICAL_STABILIZATION_MODE_OFF)

                        previewRequestBuilder = builder
                        applyCameraControls()

                        session.setRepeatingRequest(builder.build(), null, backgroundHandler)
                        onReady?.invoke()
                        Log.i(TAG, "CameraCaptureSession configured successfully")
                    } catch (e: Exception) {
                        Log.e(TAG, "Failed to start capture request", e)
                    }
                }

                override fun onConfigureFailed(session: CameraCaptureSession) {
                    Log.e(TAG, "CameraCaptureSession configuration failed")
                }
            }, backgroundHandler)

        } catch (e: Exception) {
            Log.e(TAG, "Error creating capture session", e)
        }
    }

    fun updatePreviewSurface(newSurfaceTexture: SurfaceTexture?) {
        if (newSurfaceTexture == null) {
            previewSurface?.release()
            previewSurface = null
        } else {
            previewSurface = Surface(newSurfaceTexture)
        }
        closeCaptureSession()
        createCaptureSession()
    }

    fun setZoom(factor: Float) {
        currentZoom = factor.coerceIn(1.0f, 10.0f)
        applyCameraControls()
    }

    fun setExposureCompensation(step: Int) {
        val range = currentCharacteristics?.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_RANGE) ?: Range(0, 0)
        currentExposure = step.coerceIn(range.lower, range.upper)
        applyCameraControls()
    }

    fun setFocus(autoFocus: Boolean, distance: Float = 0.0f) {
        isAutoFocus = autoFocus
        manualFocusDistance = distance.coerceIn(0.0f, 1.0f)
        applyCameraControls()
    }

    fun setTorch(enabled: Boolean) {
        isTorchOn = enabled
        applyCameraControls()
    }

    private fun applyCameraControls() {
        val builder = previewRequestBuilder ?: return
        val chars = currentCharacteristics ?: return
        val session = captureSession ?: return

        try {
            // Zoom
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val zoomRange = chars.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE)
                if (zoomRange != null) {
                    val clamped = currentZoom.coerceIn(zoomRange.lower, zoomRange.upper)
                    builder.set(CaptureRequest.CONTROL_ZOOM_RATIO, clamped)
                }
            } else {
                val sensorRect = chars.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE)
                if (sensorRect != null && currentZoom > 1.0f) {
                    val cropW = (sensorRect.width() / currentZoom).toInt()
                    val cropH = (sensorRect.height() / currentZoom).toInt()
                    val left = (sensorRect.width() - cropW) / 2
                    val top = (sensorRect.height() - cropH) / 2
                    builder.set(CaptureRequest.SCALER_CROP_REGION, Rect(left, top, left + cropW, top + cropH))
                } else if (sensorRect != null) {
                    builder.set(CaptureRequest.SCALER_CROP_REGION, sensorRect)
                }
            }

            // Exposure
            builder.set(CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION, currentExposure)

            // Focus
            if (isAutoFocus) {
                builder.set(CaptureRequest.CONTROL_AF_MODE, CameraMetadata.CONTROL_AF_MODE_CONTINUOUS_VIDEO)
            } else {
                builder.set(CaptureRequest.CONTROL_AF_MODE, CameraMetadata.CONTROL_AF_MODE_OFF)
                val minFocusDist = chars.get(CameraCharacteristics.LENS_INFO_MINIMUM_FOCUS_DISTANCE) ?: 0.0f
                builder.set(CaptureRequest.LENS_FOCUS_DISTANCE, manualFocusDistance * minFocusDist)
            }

            // Torch
            val flashAvailable = chars.get(CameraCharacteristics.FLASH_INFO_AVAILABLE) ?: false
            if (flashAvailable) {
                builder.set(
                    CaptureRequest.FLASH_MODE,
                    if (isTorchOn) CameraMetadata.FLASH_MODE_TORCH else CameraMetadata.FLASH_MODE_OFF
                )
            }

            session.setRepeatingRequest(builder.build(), null, backgroundHandler)
        } catch (e: Exception) {
            Log.e(TAG, "Error applying camera controls", e)
        }
    }

    private fun closeCaptureSession() {
        try {
            captureSession?.close()
            captureSession = null
        } catch (e: Exception) {
            Log.e(TAG, "Error closing capture session", e)
        }
    }

    fun closeCamera() {
        try {
            cameraOpenCloseLock.acquire()
            closeCaptureSession()
            cameraDevice?.close()
            cameraDevice = null
            encoderSurface = null
            previewSurface?.release()
            previewSurface = null
        } catch (e: Exception) {
            Log.e(TAG, "Error closing camera", e)
        } finally {
            cameraOpenCloseLock.release()
            stopBackgroundThread()
        }
    }
}
