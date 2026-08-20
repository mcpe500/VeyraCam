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

    iocp_ = CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket_), nullptr, 0, 0);
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

void IocpUdpServer::WorkerLoop() {
    uint8_t buffer[VEYRA_MAX_PACKET_SIZE];
#ifdef _WIN32
    while (isRunning_) {
        sockaddr_in clientAddr = {};
        int clientLen = sizeof(clientAddr);
        int bytes = recvfrom(socket_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                             reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (bytes > 0 && callback_) {
            callback_(buffer, static_cast<size_t>(bytes));
        }
    }
#else
    while (isRunning_) {
        sockaddr_in clientAddr = {};
        socklen_t clientLen = sizeof(clientAddr);
        ssize_t bytes = recvfrom(socket_, buffer, sizeof(buffer), 0,
                                 reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (bytes > 0 && callback_) {
            callback_(buffer, static_cast<size_t>(bytes));
        }
    }
#endif
}

void IocpUdpServer::Stop() {
    if (!isRunning_) return;
    isRunning_ = false;

#ifdef _WIN32
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (iocp_) {
        CloseHandle(iocp_);
        iocp_ = nullptr;
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
