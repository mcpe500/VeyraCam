package com.veyra.cam.transport

import android.util.Log
import com.veyra.cam.service.VeyraNativeBridge
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.atomic.AtomicBoolean

class UdpTransport(
    private val controlPort: Int = 5150,
    private val onControlCommandReceived: (opCode: Int, payload: JSONObject) -> Unit,
    private val onClientConnected: (clientIp: String, udpPort: Int) -> Unit,
    private val onClientDisconnected: () -> Unit
) {
    companion object {
        private const val TAG = "UdpTransport"
    }

    private var serverSocket: ServerSocket? = null
    private var clientSocket: Socket? = null
    private var controlThread: Thread? = null
    private var writer: OutputStreamWriter? = null
    private val isRunning = AtomicBoolean(false)

    fun start() {
        if (isRunning.get()) return
        isRunning.set(true)

        controlThread = Thread({
            try {
                serverSocket = ServerSocket(controlPort)
                Log.i(TAG, "TCP Control Server listening on port $controlPort")

                while (isRunning.get()) {
                    val socket = serverSocket?.accept() ?: break
                    clientSocket = socket
                    val clientIp = socket.inetAddress.hostAddress ?: "127.0.0.1"
                    Log.i(TAG, "Client connected from $clientIp:${socket.port}")

                    val reader = BufferedReader(InputStreamReader(socket.getInputStream()))
                    writer = OutputStreamWriter(socket.getOutputStream())

                    // Default UDP destination is client IP and port 5151
                    onClientConnected(clientIp, 5151)

                    while (isRunning.get() && !socket.isClosed) {
                        val line = reader.readLine() ?: break
                        try {
                            val json = JSONObject(line)
                            val type = json.optString("type")
                            val opCode = when (type) {
                                "hello" -> 0x01
                                "capabilities" -> 0x02
                                "startStream" -> 0x03
                                "stopStream" -> 0x04
                                "setZoom" -> 0x05
                                "setExposure" -> 0x06
                                "setFocus" -> 0x07
                                "requestIdr" -> 0x08
                                "setBitrate" -> 0x09
                                "ping" -> 0x0B
                                else -> 0x00
                            }
                            onControlCommandReceived(opCode, json)
                        } catch (e: Exception) {
                            Log.e(TAG, "Error parsing control JSON: $line", e)
                        }
                    }

                    Log.i(TAG, "Client disconnected")
                    onClientDisconnected()
                }
            } catch (e: Exception) {
                if (isRunning.get()) {
                    Log.e(TAG, "Error in TCP control server", e)
                }
            }
        }, "VeyraTcpControlThread").apply { start() }
    }

    fun sendControlMessage(json: JSONObject) {
        try {
            val w = writer ?: return
            synchronized(w) {
                w.write(json.toString() + "\n")
                w.flush()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send control message", e)
        }
    }

    fun stop() {
        isRunning.set(false)
        try {
            clientSocket?.close()
            clientSocket = null
            serverSocket?.close()
            serverSocket = null
        } catch (e: Exception) {
            Log.e(TAG, "Error closing sockets", e)
        }

        try {
            controlThread?.interrupt()
            controlThread?.join(1000)
            controlThread = null
        } catch (e: InterruptedException) {
            Log.e(TAG, "Error joining control thread", e)
        }
        Log.i(TAG, "UdpTransport stopped")
    }
}
