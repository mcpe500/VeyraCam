#include "win_rfcomm.h"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2bth.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace veyra {

WinRfcommClient::WinRfcommClient() = default;

WinRfcommClient::~WinRfcommClient() {
    Disconnect();
}

std::vector<BluetoothDeviceInfo> WinRfcommClient::DiscoverPairedDevices() {
    std::vector<BluetoothDeviceInfo> devices;
#ifdef _WIN32
    // Enumerate paired Bluetooth classic devices
#endif
    return devices;
}

bool WinRfcommClient::Connect(const std::string& bluetoothAddress, DataReceivedCallback onData) {
    Disconnect();
    address_ = bluetoothAddress;
    callback_ = onData;

#ifdef _WIN32
    SOCKET s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (s == INVALID_SOCKET) {
        return false;
    }

    SOCKADDR_BTH sa = {};
    sa.addressFamily = AF_BTH;
    sa.port = 0; // RFCOMM channel
    // GUID matching mobile service: a888c728-6623-4217-9160-b6f2048995a9
    sa.serviceClassId = { 0xa888c728, 0x6623, 0x4217, { 0x91, 0x60, 0xb6, 0xf2, 0x04, 0x89, 0x95, 0xa9 } };

    if (connect(s, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == SOCKET_ERROR) {
        closesocket(s);
        return false;
    }
    socket_ = s;
    isConnected_ = true;
    readThread_ = std::thread(&WinRfcommClient::ReadLoop, this);
    return true;
#else
    return false;
#endif
}

bool WinRfcommClient::Send(const uint8_t* data, size_t size) {
    if (!isConnected_ || !data || size == 0) return false;
    std::lock_guard<std::mutex> lock(sendMutex_);

#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(socket_);
    int res = send(s, reinterpret_cast<const char*>(data), static_cast<int>(size), 0);
    return res != SOCKET_ERROR;
#else
    return false;
#endif
}

void WinRfcommClient::ReadLoop() {
#ifdef _WIN32
    uint8_t buffer[2048];
    while (isConnected_) {
        SOCKET s = static_cast<SOCKET>(socket_);
        int bytes = recv(s, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
        if (bytes <= 0) break;

        if (callback_) {
            callback_(buffer, static_cast<size_t>(bytes));
        }
    }
#endif
    isConnected_ = false;
}

void WinRfcommClient::Disconnect() {
    if (!isConnected_) return;
    isConnected_ = false;

#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(socket_);
    if (s != INVALID_SOCKET) {
        closesocket(s);
        socket_ = static_cast<uintptr_t>(INVALID_SOCKET);
    }
#endif

    if (readThread_.joinable()) {
        readThread_.join();
    }
}

} // namespace veyra
