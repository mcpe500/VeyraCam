#include "tcp_control_server.h"
#include <iostream>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace veyra {

TcpControlClient::TcpControlClient() = default;

TcpControlClient::~TcpControlClient() {
    Disconnect();
}

bool TcpControlClient::Connect(const std::string& host, uint16_t port, MessageReceivedCallback onMessage) {
    Disconnect();

    host_ = host;
    port_ = port;
    callback_ = onMessage;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr);

    if (connect(s, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(s);
        return false;
    }
    socket_ = s;
#else
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return false;

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr);

    if (connect(s, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        close(s);
        return false;
    }
    socket_ = s;
#endif

    isConnected_ = true;
    readThread_ = std::thread(&TcpControlClient::ReadLoop, this);
    std::cout << "[TcpControlClient] Connected to " << host_ << ":" << port_ << std::endl;
    return true;
}

bool TcpControlClient::SendControlMessage(const std::string& jsonMessage) {
    if (!isConnected_) return false;
    std::lock_guard<std::mutex> lock(sendMutex_);

    std::string msgWithNewline = jsonMessage + "\n";
#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(socket_);
    int res = send(s, msgWithNewline.c_str(), static_cast<int>(msgWithNewline.length()), 0);
    return res != SOCKET_ERROR;
#else
    int s = static_cast<int>(socket_);
    ssize_t res = send(s, msgWithNewline.c_str(), msgWithNewline.length(), 0);
    return res > 0;
#endif
}

void TcpControlClient::ReadLoop() {
    char buffer[4096];
    std::string accumulated;

    while (isConnected_) {
#ifdef _WIN32
        SOCKET s = static_cast<SOCKET>(socket_);
        int bytes = recv(s, buffer, sizeof(buffer) - 1, 0);
#else
        int s = static_cast<int>(socket_);
        ssize_t bytes = recv(s, buffer, sizeof(buffer) - 1, 0);
#endif
        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        accumulated += buffer;

        size_t pos;
        while ((pos = accumulated.find('\n')) != std::string::npos) {
            std::string line = accumulated.substr(0, pos);
            accumulated.erase(0, pos + 1);

            if (!line.empty() && callback_) {
                callback_(line);
            }
        }
    }

    isConnected_ = false;
}

void TcpControlClient::Disconnect() {
    if (!isConnected_) return;
    isConnected_ = false;

#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(socket_);
    if (s != INVALID_SOCKET) {
        closesocket(s);
        socket_ = static_cast<uintptr_t>(INVALID_SOCKET);
    }
#else
    int s = static_cast<int>(socket_);
    if (s >= 0) {
        close(s);
        socket_ = -1;
    }
#endif

    if (readThread_.joinable()) {
        readThread_.join();
    }
}

} // namespace veyra
