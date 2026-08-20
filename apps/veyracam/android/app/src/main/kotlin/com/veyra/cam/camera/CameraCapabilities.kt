package com.veyra.cam.camera

import android.content.Context
import android.graphics.ImageFormat
import android.graphics.SurfaceTexture
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CameraMetadata
import android.os.Build
import android.util.Range
import android.util.Size
import org.json.JSONArray
import org.json.JSONObject

data class CameraInfo(
    val cameraId: String,
    val isFacingBack: Boolean,
    val supportedResolutions: List<Size>,
    val supportedFpsRanges: List<Range<Int>>,
    val maxZoomRatio: Float,
    val hasFlash: Boolean,
    val supportsManualFocus: Boolean,
    val exposureRange: Range<Int>,
    val exposureStepRational: Float
)

class CameraCapabilities(private val context: Context) {
    private val cameraManager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager

    fun inspectAllCameras(): List<CameraInfo> {
        val result = mutableListOf<CameraInfo>()
        try {
            val cameraIds = cameraManager.cameraIdList
            for (id in cameraIds) {
                val chars = cameraManager.getCameraCharacteristics(id)
                val facing = chars.get(CameraCharacteristics.LENS_FACING)
                val isBack = facing == CameraCharacteristics.LENS_FACING_BACK

                val map = chars.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
                val resolutions = map?.getOutputSizes(SurfaceTexture::class.java)?.toList() ?: emptyList()

                val fpsRanges = chars.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)?.toList() ?: emptyList()

                val maxZoom = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    chars.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE)?.upper ?: 1.0f
                } else {
                    chars.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM) ?: 1.0f
                }

                val hasFlash = chars.get(CameraCharacteristics.FLASH_INFO_AVAILABLE) ?: false
                val afModes = chars.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES) ?: intArrayOf()
                val supportsManualFocus = afModes.contains(CameraMetadata.CONTROL_AF_MODE_OFF)

                val exposureRange = chars.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_RANGE) ?: Range(0, 0)
                val exposureStep = chars.get(CameraCharacteristics.CONTROL_AE_COMPENSATION_STEP)?.toFloat() ?: 1.0f

                result.add(
                    CameraInfo(
                        cameraId = id,
                        isFacingBack = isBack,
                        supportedResolutions = resolutions,
                        supportedFpsRanges = fpsRanges,
                        maxZoomRatio = maxZoom,
                        hasFlash = hasFlash,
                        supportsManualFocus = supportsManualFocus,
                        exposureRange = exposureRange,
                        exposureStepRational = exposureStep
                    )
                )
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return result
    }

    fun toCapabilitiesJson(deviceName: String, deviceId: String): JSONObject {
        val cameras = inspectAllCameras()
        val json = JSONObject()
        json.put("type", "capabilities")
        json.put("protocol", 1)

        val device = JSONObject()
        device.put("name", deviceName)
        device.put("id", deviceId)
        device.put("model", Build.MODEL)
        device.put("android_version", Build.VERSION.SDK_INT)
        json.put("device", device)

        val camArray = JSONArray()
        for (cam in cameras) {
            val camObj = JSONObject()
            camObj.put("id", cam.cameraId)
            camObj.put("facing", if (cam.isFacingBack) "back" else "front")
            camObj.put("max_zoom", cam.maxZoomRatio)
            camObj.put("has_flash", cam.hasFlash)
            camObj.put("manual_focus", cam.supportsManualFocus)
            camObj.put("min_exposure", cam.exposureRange.lower)
            camObj.put("max_exposure", cam.exposureRange.upper)

            val resArray = JSONArray()
            for (res in cam.supportedResolutions) {
                resArray.put("${res.width}x${res.height}")
            }
            camObj.put("resolutions", resArray)

            val fpsArray = JSONArray()
            for (fps in cam.supportedFpsRanges) {
                fpsArray.put("${fps.lower}-${fps.upper}")
            }
            camObj.put("fps_ranges", fpsArray)

            camArray.put(camObj)
        }
        json.put("cameras", camArray)
        return json
    }
}
