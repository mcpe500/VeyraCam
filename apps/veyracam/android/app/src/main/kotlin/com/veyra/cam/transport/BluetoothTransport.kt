package com.veyra.cam.transport

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothServerSocket
import android.bluetooth.BluetoothSocket
import android.util.Log
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean

class BluetoothTransport(
    private val onDataReceived: (data: ByteArray, length: Int) -> Unit,
    private val onConnected: () -> Unit,
    private val onDisconnected: () -> Unit
) {
    companion object {
        private const val TAG = "BluetoothTransport"
        private const val SERVICE_NAME = "VeyraBluetoothService"
        val VEYRA_BT_UUID: UUID = UUID.fromString("a888c728-6623-4217-9160-b6f2048995a9")
    }

    private val bluetoothAdapter: BluetoothAdapter? = BluetoothAdapter.getDefaultAdapter()
    private var serverSocket: BluetoothServerSocket? = null
    private var clientSocket: BluetoothSocket? = null
    private var listenThread: Thread? = null
    private var outputStream: OutputStream? = null
    private val isRunning = AtomicBoolean(false)

    @SuppressLint("MissingPermission")
    fun start() {
        val adapter = bluetoothAdapter
        if (adapter == null || !adapter.isEnabled) {
            Log.w(TAG, "Bluetooth not available or not enabled")
            return
        }

        if (isRunning.get()) return
        isRunning.set(true)

        listenThread = Thread({
            try {
                serverSocket = adapter.listenUsingRfcommWithServiceRecord(SERVICE_NAME, VEYRA_BT_UUID)
                Log.i(TAG, "Bluetooth RFCOMM server listening on UUID: $VEYRA_BT_UUID")

                while (isRunning.get()) {
                    val socket = serverSocket?.accept() ?: break
                    clientSocket = socket
                    outputStream = socket.outputStream
                    val inputStream: InputStream = socket.inputStream
                    Log.i(TAG, "Bluetooth client connected: ${socket.remoteDevice?.name}")

                    onConnected()

                    val buffer = ByteArray(2048)
                    while (isRunning.get() && socket.isConnected) {
                        val bytesRead = inputStream.read(buffer)
                        if (bytesRead <= 0) break
                        onDataReceived(buffer, bytesRead)
                    }

                    Log.i(TAG, "Bluetooth client disconnected")
                    onDisconnected()
                }
            } catch (e: Exception) {
                if (isRunning.get()) {
                    Log.e(TAG, "Error in Bluetooth RFCOMM listener", e)
                }
            }
        }, "VeyraBluetoothRfcommThread").apply { start() }
    }

    fun sendData(data: ByteArray, offset: Int = 0, length: Int = data.size) {
        try {
            val stream = outputStream ?: return
            synchronized(stream) {
                stream.write(data, offset, length)
                stream.flush()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to write Bluetooth data", e)
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
            Log.e(TAG, "Error closing Bluetooth sockets", e)
        }

        try {
            listenThread?.interrupt()
            listenThread?.join(1000)
            listenThread = null
        } catch (e: InterruptedException) {
            Log.e(TAG, "Error joining Bluetooth thread", e)
        }
        Log.i(TAG, "BluetoothTransport stopped")
    }
}
