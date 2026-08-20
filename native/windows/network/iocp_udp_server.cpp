#include "iocp_udp_server.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace veyra {

IocpUdpServer::IocpUdpServer(uint16_t port) : port_(port) {}

IocpUdpServer::~IocpUdpServer() {
    Stop();
}

bool IocpUdpServer::Start(PacketReceivedCallback onPacketReceived) {
    if (isRunning_) return true;
    callback_ = onPacketReceived;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    socket_ = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (socket_ == INVALID_SOCKET) {
        std::cerr << "[IocpUdpServer] Failed to create socket" << std::endl;
        return false;
    }

    int rcvBuf = 2 * 1024 * 1024; // 2 MB receive buffer
    setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBuf), sizeof(rcvBuf));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(socket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[IocpUdpServer] Failed to bind socket on port " << port_ << std::endl;
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }
#else
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ < 0) return false;

    int rcvBuf = 2 * 1024 * 1024;
    setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, &rcvBuf, sizeof(rcvBuf));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(socket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        close(socket_);
        socket_ = -1;
        return false;
    }
#endif

    isRunning_ = true;
    workerThread_ = std::thread(&IocpUdpServer::WorkerLoop, this);
    std::cout << "[IocpUdpServer] Started UDP listening on port " << port_ << std::endl;
    return true;
}

void IocpUdpServer::SetExpectedPeer(const std::string& hostIp) {
    std::lock_guard<std::mutex> lock(peerMutex_);
    expectedPeer_ = hostIp;
    expectedPeerAddr_ = 0;
    if (!hostIp.empty()) {
        in_addr addr{};
        if (inet_pton(AF_INET, hostIp.c_str(), &addr) == 1) {
            expectedPeerAddr_ = addr.s_addr;
        }
    }
}

void IocpUdpServer::WorkerLoop() {
    uint8_t buffer[VEYRA_MAX_PACKET_SIZE];
    while (isRunning_) {
        sockaddr_in clientAddr = {};
        int clientLen = sizeof(clientAddr);
        int bytes = static_cast<int>(recvfrom(
#ifdef _WIN32
            socket_,
#else
            static_cast<int>(socket_),
#endif
            reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
            reinterpret_cast<sockaddr*>(&clientAddr), &clientLen));
        if (bytes <= 0 || !callback_) {
            continue;
        }

        // H-3: reject media packets from any peer other than the paired device.
        uint32_t peerAddr = 0;
        {
            std::lock_guard<std::mutex> lock(peerMutex_);
            if (expectedPeerAddr_ != 0) {
                peerAddr = expectedPeerAddr_;
            }
        }
        if (peerAddr != 0 && clientAddr.sin_addr.s_addr != peerAddr) {
            continue; // drop packet from unpaired source
        }

        callback_(buffer, static_cast<size_t>(bytes));
    }
}

void IocpUdpServer::Stop() {
    if (!isRunning_) return;
    isRunning_ = false;

#ifdef _WIN32
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    WSACleanup();
#else
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
#endif

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    std::cout << "[IocpUdpServer] Stopped" << std::endl;
}

} // namespace veyra
