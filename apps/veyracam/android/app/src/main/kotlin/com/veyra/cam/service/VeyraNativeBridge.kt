package com.veyra.cam.service

import java.nio.ByteBuffer

object VeyraNativeBridge {
    init {
        try {
            System.loadLibrary("veyra_jni")
        } catch (e: UnsatisfiedLinkError) {
            e.printStackTrace()
        }
    }

    external fun nativeCreateSession(sessionId: Int): Long
    external fun nativeDestroySession(handle: Long)
    external fun nativeConfigureUdpDestination(handle: Long, host: String, port: Int): Boolean
    external fun nativeSendVideoNal(
        handle: Long,
        buffer: ByteBuffer,
        offset: Int,
        length: Int,
        isKeyframe: Boolean,
        timestampUs: Long
    )
    external fun nativeSendAudioFrame(
        handle: Long,
        buffer: ByteBuffer,
        offset: Int,
        length: Int,
        timestampUs: Long
    )
    external fun nativeGetTelemetryJson(handle: Long): String
}
