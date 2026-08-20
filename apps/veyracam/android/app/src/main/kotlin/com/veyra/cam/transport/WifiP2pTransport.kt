package com.veyra.cam.transport

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.NetworkInfo
import android.net.wifi.p2p.*
import android.os.Looper
import android.util.Log

class WifiP2pTransport(
    private val context: Context,
    private val onConnectionEstablished: (groupOwnerAddress: String) -> Unit
) {
    companion object {
        private const val TAG = "WifiP2pTransport"
    }

    private val wifiP2pManager = context.getSystemService(Context.WIFI_P2P_SERVICE) as? WifiP2pManager
    private var channel: WifiP2pManager.Channel? = null
    private var receiver: BroadcastReceiver? = null

    @SuppressLint("MissingPermission")
    fun initialize() {
        val manager = wifiP2pManager ?: return
        channel = manager.initialize(context, Looper.getMainLooper(), null)

        val intentFilter = IntentFilter().apply {
            addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)
            addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION)
        }

        receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context?, intent: Intent?) {
                when (intent?.action) {
                    WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION -> {
                        val networkInfo = intent.getParcelableExtra<NetworkInfo>(WifiP2pManager.EXTRA_NETWORK_INFO)
                        if (networkInfo?.isConnected == true) {
                            manager.requestConnectionInfo(channel) { info ->
                                if (info.groupFormed) {
                                    val ownerIp = info.groupOwnerAddress?.hostAddress ?: "192.168.49.1"
                                    Log.i(TAG, "Wi-Fi Direct Connected! Group Owner: $ownerIp")
                                    onConnectionEstablished(ownerIp)
                                }
                            }
                        }
                    }
                }
            }
        }

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU) {
            context.registerReceiver(receiver, intentFilter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            context.registerReceiver(receiver, intentFilter)
        }
        startDiscovery()
    }

    @SuppressLint("MissingPermission")
    fun startDiscovery() {
        val manager = wifiP2pManager ?: return
        val ch = channel ?: return
        manager.discoverPeers(ch, object : WifiP2pManager.ActionListener {
            override fun onSuccess() {
                Log.i(TAG, "Wi-Fi Direct Peer Discovery Started")
            }

            override fun onFailure(reason: Int) {
                Log.w(TAG, "Wi-Fi Direct Peer Discovery Failed: $reason")
            }
        })
    }

    fun stop() {
        try {
            receiver?.let { context.unregisterReceiver(it) }
            receiver = null
        } catch (_: Exception) {}
        Log.i(TAG, "WifiP2pTransport stopped")
    }
}
