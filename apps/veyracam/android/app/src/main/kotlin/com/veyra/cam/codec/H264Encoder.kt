package com.veyra.cam.codec

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface
import com.veyra.cam.service.VeyraNativeBridge
import java.nio.ByteBuffer

class H264Encoder(
    private val width: Int = 1280,
    private val height: Int = 720,
    private val fps: Int = 30,
    private var bitrateBps: Int = 2500000,
    private val nativeHandleProvider: () -> Long
) {
    companion object {
        private const val TAG = "H264Encoder"
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC
    }

    private var mediaCodec: MediaCodec? = null
    private var inputSurface: Surface? = null
    private var encoderThread: HandlerThread? = null
    private var encoderHandler: Handler? = null

    @Volatile
    private var isRunning: Boolean = false

    fun start(): Surface {
        encoderThread = HandlerThread("VeyraEncoderThread").apply { start() }
        encoderHandler = Handler(encoderThread!!.looper)

        val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height).apply {
            setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
            setInteger(MediaFormat.KEY_BIT_RATE, bitrateBps)
            setInteger(MediaFormat.KEY_FRAME_RATE, fps)
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
            setInteger(MediaFormat.KEY_BITRATE_MODE, MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR)

            // Low latency encoder hints
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                setInteger(MediaFormat.KEY_LATENCY, 0)
                setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
            } else {
                try {
                    setInteger("latency", 0)
                } catch (_: Exception) {}
            }

            // Baseline profile for universal compatibility & low latency
            setInteger(MediaFormat.KEY_PROFILE, MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline)
            setInteger(MediaFormat.KEY_LEVEL, MediaCodecInfo.CodecProfileLevel.AVCLevel31)
        }

        val codec = MediaCodec.createEncoderByType(MIME_TYPE)
        mediaCodec = codec

        codec.setCallback(object : MediaCodec.Callback() {
            override fun onInputBufferAvailable(codec: MediaCodec, index: Int) {
                // Not used with Surface input
            }

            override fun onOutputBufferAvailable(
                codec: MediaCodec,
                index: Int,
                info: MediaCodec.BufferInfo
            ) {
                if (!isRunning) return

                val outputBuffer: ByteBuffer? = codec.getOutputBuffer(index)
                if (outputBuffer != null && info.size > 0) {
                    outputBuffer.position(info.offset)
                    outputBuffer.limit(info.offset + info.size)

                    val isKeyframe = (info.flags and MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0 ||
                                     (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0

                    val handle = nativeHandleProvider()
                    if (handle != 0L) {
                        VeyraNativeBridge.nativeSendVideoNal(
                            handle,
                            outputBuffer,
                            info.offset,
                            info.size,
                            isKeyframe,
                            info.presentationTimeUs
                        )
                    }
                }
                codec.releaseOutputBuffer(index, false)
            }

            override fun onError(codec: MediaCodec, e: MediaCodec.CodecException) {
                Log.e(TAG, "MediaCodec Error: ${e.diagnosticInfo}", e)
            }

            override fun onOutputFormatChanged(codec: MediaCodec, format: MediaFormat) {
                Log.i(TAG, "MediaCodec output format changed: $format")
            }
        }, encoderHandler)

        codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        inputSurface = codec.createInputSurface()
        codec.start()
        isRunning = true

        Log.i(TAG, "MediaCodec H.264 Encoder started ($width x $height @ $fps fps, $bitrateBps bps)")
        return inputSurface!!
    }

    fun requestIdr() {
        val codec = mediaCodec ?: return
        try {
            val params = Bundle().apply {
                putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0)
            }
            codec.setParameters(params)
            Log.d(TAG, "Requested IDR keyframe from MediaCodec")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to request IDR keyframe", e)
        }
    }

    fun updateBitrate(newBitrateBps: Int) {
        val codec = mediaCodec ?: return
        try {
            bitrateBps = newBitrateBps
            val params = Bundle().apply {
                putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, newBitrateBps)
            }
            codec.setParameters(params)
            Log.i(TAG, "Updated MediaCodec bitrate to $newBitrateBps bps")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to update bitrate", e)
        }
    }

    fun stop() {
        isRunning = false
        try {
            mediaCodec?.stop()
            mediaCodec?.release()
            mediaCodec = null
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing MediaCodec", e)
        }

        inputSurface?.release()
        inputSurface = null

        encoderThread?.quitSafely()
        try {
            encoderThread?.join()
            encoderThread = null
            encoderHandler = null
        } catch (e: InterruptedException) {
            Log.e(TAG, "Error joining encoder thread", e)
        }
        Log.i(TAG, "MediaCodec H.264 Encoder stopped")
    }
}
