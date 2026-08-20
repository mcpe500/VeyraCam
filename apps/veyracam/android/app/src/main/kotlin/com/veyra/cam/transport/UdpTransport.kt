package com.veyra.cam.transport

import android.util.Log
import com.veyra.cam.service.PairingManager
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.atomic.AtomicBoolean

class UdpTransport(
    private val controlPort: Int = 5150,
    private val pairingManager: PairingManager,
    private val getServerPublicKey: () -> String,
    private val getSessionId: () -> Int,
    private val onControlCommandReceived: (opCode: Int, payload: JSONObject) -> Unit,
    private val onAuthenticated: (clientIp: String, udpPort: Int) -> Unit,
    private val onPinRequired: (pin: String) -> Unit,
    private val onPairingFailed: (reason: String) -> Unit,
    private val onCompletePairing: (clientPubKey: String) -> Boolean,
    private val onClientDisconnected: () -> Unit
) {
    companion object {
        private const val TAG = "UdpTransport"
        private const val UDP_MEDIA_PORT = 5151
        private const val MAX_LINE_LENGTH = 64 * 1024 // L-2: cap control line size
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

                    val reader = BufferedReader(InputStreamReader(socket.getInputStream()), 8192)
                    writer = OutputStreamWriter(socket.getOutputStream())

                    handleConnection(socket, reader, clientIp)
                }
            } catch (e: Exception) {
                if (isRunning.get()) {
                    Log.e(TAG, "Error in TCP control server", e)
                }
            }
        }, "VeyraTcpControlThread").apply { start() }
    }

    // C-2: pairing challenge/response + token-gated control commands.
    private fun handleConnection(socket: Socket, reader: BufferedReader, clientIp: String) {
        val pin = pairingManager.startPairing(clientIp) ?: run {
            Log.w(TAG, "Pairing manager not ready (locked); rejecting connection")
            sendControlMessage(JSONObject().put("type", "pairing_error").put("reason", "locked"))
            try { socket.close() } catch (_: Exception) {}
            return
        }

        onPinRequired(pin)

        val challenge = JSONObject()
            .put("type", "pairing_challenge")
            .put("server_pubkey", getServerPublicKey())
            .put("session_id", getSessionId())
        sendControlMessage(challenge)

        var authenticated = false

        try {
            while (isRunning.get() && !socket.isClosed) {
                val line = reader.readLine() ?: break
                if (line.length > MAX_LINE_LENGTH) {
                    Log.w(TAG, "Oversized control line dropped (${line.length} bytes)")
                    continue
                }
                try {
                    val json = JSONObject(line)
                    val type = json.optString("type")

                    if (type == "pairing_response") {
                        if (authenticated) continue
                        val pinAttempt = json.optString("pin", "")
                        val clientPubKey = json.optString("client_pubkey", "")

                        when (pairingManager.verifyPin(pinAttempt)) {
                            PairingManager.PairingResult.OK -> {
                                if (!clientPubKey.isEmpty() && onPairingComplete(clientPubKey)) {
                                    val token = pairingManager.getAuthToken()
                                    sendControlMessage(JSONObject().put("type", "pairing_ok").put("token", token))
                                    authenticated = true
                                    onAuthenticated(clientIp, UDP_MEDIA_PORT)
                                } else {
                                    sendControlMessage(JSONObject().put("type", "pairing_error").put("reason", "invalid_key"))
                                }
                            }
                            PairingManager.PairingResult.INVALID_PIN -> {
                                sendControlMessage(JSONObject().put("type", "pairing_error").put("reason", "invalid_pin"))
                            }
                            PairingManager.PairingResult.LOCKED -> {
                                sendControlMessage(JSONObject().put("type", "pairing_error").put("reason", "locked"))
                                break
                            }
                            PairingManager.PairingResult.NOT_AWAITING -> { /* ignore */ }
                        }
                        continue
                    }

                    if (!authenticated) {
                        Log.w(TAG, "Rejected pre-auth control command: $type")
                        continue
                    }

                    // Every post-auth command must carry the pairing token.
                    val token = json.optString("auth_token", "")
                    if (!pairingManager.isTokenValid(token)) {
                        Log.w(TAG, "Invalid auth_token on command: $type")
                        continue
                    }

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
        } catch (e: Exception) {
            Log.e(TAG, "Error in control connection loop", e)
        }

        Log.i(TAG, "Client disconnected")
        pairingManager.reset()
        onClientDisconnected()
    }

    private fun onPairingComplete(clientPubKey: String): Boolean {
        val completed = onCompletePairing(clientPubKey)
        if (!completed) {
            onPairingFailed("key_derivation_failed")
        }
        return completed
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