#include "../include/Common.h"
#include <sstream>
#include <ctime>
#include <iomanip>

// 初始化网络库
bool InitNetwork() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        PrintError("WSAStartup failed");
        return false;
    }
#endif
    return true;
}

// 清理网络库
void CleanupNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// 发送消息
bool SendMessage(SocketType sock, const Message& msg) {
    // 发送消息头
    if (send(sock, reinterpret_cast<const char*>(&msg.header), sizeof(MessageHeader), 0) == SOCKET_ERROR_VALUE) {
        return false;
    }
    
    // 发送发送者用户名
    if (msg.header.senderLen > 0) {
        if (send(sock, msg.sender.c_str(), msg.header.senderLen, 0) == SOCKET_ERROR_VALUE) {
            return false;
        }
    }
    
    // 发送消息内容
    if (msg.header.contentLen > 0) {
        if (send(sock, msg.content.c_str(), msg.header.contentLen, 0) == SOCKET_ERROR_VALUE) {
            return false;
        }
    }
    
    return true;
}

// 接收消息
bool ReceiveMessage(SocketType sock, Message& msg) {
    // 接收消息头
    int bytesReceived = recv(sock, reinterpret_cast<char*>(&msg.header), sizeof(MessageHeader), 0);
    if (bytesReceived <= 0) {
        return false;
    }
    
    // 检查消息长度是否合法
    if (msg.header.senderLen < 0 || msg.header.senderLen > MAX_MESSAGE_SIZE ||
        msg.header.contentLen < 0 || msg.header.contentLen > MAX_MESSAGE_SIZE) {
        return false;
    }
    
    // 接收发送者用户名
    if (msg.header.senderLen > 0) {
        char* senderBuffer = new char[msg.header.senderLen + 1];
        bytesReceived = recv(sock, senderBuffer, msg.header.senderLen, 0);
        if (bytesReceived <= 0) {
            delete[] senderBuffer;
            return false;
        }
        senderBuffer[msg.header.senderLen] = '\0';
        msg.sender = std::string(senderBuffer);
        delete[] senderBuffer;
    }
    
    // 接收消息内容
    if (msg.header.contentLen > 0) {
        char* contentBuffer = new char[msg.header.contentLen + 1];
        bytesReceived = recv(sock, contentBuffer, msg.header.contentLen, 0);
        if (bytesReceived <= 0) {
            delete[] contentBuffer;
            return false;
        }
        contentBuffer[msg.header.contentLen] = '\0';
        msg.content = std::string(contentBuffer);
        delete[] contentBuffer;
    }
    
    return true;
}

// 打印错误信息
void PrintError(const std::string& msg) {
#ifdef _WIN32
    std::cerr << msg << " Error code: " << WSAGetLastError() << std::endl;
#else
    std::cerr << msg << " Error: " << strerror(errno) << std::endl;
#endif
}

// 获取当前时间字符串
std::string GetCurrentTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    
#ifdef _WIN32
    localtime_s(&timeinfo, &now);
#else
    localtime_r(&now, &timeinfo);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%H:%M:%S");
    return oss.str();
}

// 字符串分割函数
std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

// 检查用户名是否合法
bool IsValidUsername(const std::string& username) {
    if (username.empty() || username.length() > MAX_USERNAME_LEN) {
        return false;
    }
    
    // 可以添加更多验证规则,如检查非法字符等
    for (char c : username) {
        if (c == ',' || c == '\n' || c == '\r') {
            return false;
        }
    }
    
    return true;
}
