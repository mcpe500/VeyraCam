package com.veyra.cam.audio

import android.annotation.SuppressLint
import android.media.*
import android.os.Build
import android.util.Log
import com.veyra.cam.service.VeyraNativeBridge
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean

class AudioEngine(
    private val sampleRate: Int = 48000,
    private val channelCount: Int = 1,
    private val bitrateBps: Int = 32000,
    private val nativeHandleProvider: () -> Long
) {
    companion object {
        private const val TAG = "AudioEngine"
        private const val MIME_TYPE = MediaFormat.MIMETYPE_AUDIO_AAC
    }

    private var audioRecord: AudioRecord? = null
    private var mediaCodec: MediaCodec? = null
    private var recordingThread: Thread? = null
    private val isRunning = AtomicBoolean(false)

    @SuppressLint("MissingPermission")
    fun start() {
        if (isRunning.get()) return

        val channelConfig = if (channelCount == 1) AudioFormat.CHANNEL_IN_MONO else AudioFormat.CHANNEL_IN_STEREO
        val audioFormat = AudioFormat.ENCODING_PCM_16BIT
        val minBufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat)
        val bufferSize = (minBufferSize * 2).coerceAtLeast(4096)

        try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.VOICE_COMMUNICATION,
                sampleRate,
                channelConfig,
                audioFormat,
                bufferSize
            )

            // Setup AAC audio encoder
            val format = MediaFormat.createAudioFormat(MIME_TYPE, sampleRate, channelCount).apply {
                setInteger(MediaFormat.KEY_BIT_RATE, bitrateBps)
                setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
                setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, bufferSize)
            }

            mediaCodec = MediaCodec.createEncoderByType(MIME_TYPE).apply {
                configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
                start()
            }

            if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
                Log.e(TAG, "AudioRecord failed to initialize")
                stop()
                return
            }

            audioRecord?.startRecording()
            isRunning.set(true)

            recordingThread = Thread({
                processAudioLoop(bufferSize)
            }, "VeyraAudioRecordThread").apply { start() }

            Log.i(TAG, "AudioEngine started ($sampleRate Hz, $channelCount ch, $bitrateBps bps)")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start AudioEngine", e)
            stop()
        }
    }

    private fun processAudioLoop(bufferSize: Int) {
        val pcmBuffer = ByteArray(bufferSize)
        val bufferInfo = MediaCodec.BufferInfo()

        while (isRunning.get()) {
            val record = audioRecord ?: break
            val codec = mediaCodec ?: break

            val readBytes = record.read(pcmBuffer, 0, pcmBuffer.size)
            if (readBytes > 0) {
                val inputIndex = codec.dequeueInputBuffer(5000)
                if (inputIndex >= 0) {
                    val inputBuffer = codec.getInputBuffer(inputIndex)
                    if (inputBuffer != null) {
                        inputBuffer.clear()
                        inputBuffer.put(pcmBuffer, 0, readBytes)
                        val timestampUs = System.nanoTime() / 1000
                        codec.queueInputBuffer(inputIndex, 0, readBytes, timestampUs, 0)
                    }
                }
            }

            var outputIndex = codec.dequeueOutputBuffer(bufferInfo, 0)
            while (outputIndex >= 0) {
                val outputBuffer = codec.getOutputBuffer(outputIndex)
                if (outputBuffer != null && bufferInfo.size > 0) {
                    outputBuffer.position(bufferInfo.offset)
                    outputBuffer.limit(bufferInfo.offset + bufferInfo.size)

                    val handle = nativeHandleProvider()
                    if (handle != 0L) {
                        VeyraNativeBridge.nativeSendAudioFrame(
                            handle,
                            outputBuffer,
                            bufferInfo.offset,
                            bufferInfo.size,
                            bufferInfo.presentationTimeUs
                        )
                    }
                }
                codec.releaseOutputBuffer(outputIndex, false)
                outputIndex = codec.dequeueOutputBuffer(bufferInfo, 0)
            }
        }
    }

    fun stop() {
        isRunning.set(false)
        try {
            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing AudioRecord", e)
        }

        try {
            mediaCodec?.stop()
            mediaCodec?.release()
            mediaCodec = null
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing Audio MediaCodec", e)
        }

        try {
            recordingThread?.join(1000)
            recordingThread = null
        } catch (e: InterruptedException) {
            Log.e(TAG, "Error joining audio thread", e)
        }
        Log.i(TAG, "AudioEngine stopped")
    }
}
