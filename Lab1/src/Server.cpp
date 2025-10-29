#include "../include/Common.h"
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

// 客户端信息结构
struct ClientInfo {
    SocketType socket;
    std::string username;
    std::thread thread;
    
    ClientInfo(SocketType s, const std::string& name) 
        : socket(s), username(name) {}
};

// 全局变量
std::map<SocketType, ClientInfo*> g_clients;  // 客户端映射表
std::mutex g_clientsMutex;                     // 客户端列表互斥锁
bool g_serverRunning = true;                   // 服务器运行标志

// 函数声明
void HandleClient(ClientInfo* clientInfo);
void BroadcastMessage(const Message& msg, SocketType excludeSocket = INVALID_SOCKET_VALUE);
void SendUserList(SocketType socket);
std::string GetOnlineUsers();
void RemoveClient(SocketType socket);

// 广播消息给所有客户端(可以排除某个客户端)
void BroadcastMessage(const Message& msg, SocketType excludeSocket) {
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    
    for (auto& pair : g_clients) {
        if (pair.first != excludeSocket) {
            if (!SendMessage(pair.first, msg)) {
                std::cout << "[" << GetCurrentTime() << "] Failed to send message to " 
                         << pair.second->username << std::endl;
            }
        }
    }
}

// 获取在线用户列表
std::string GetOnlineUsers() {
    std::string userList;
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    
    for (auto& pair : g_clients) {
        if (!userList.empty()) {
            userList += ",";
        }
        userList += pair.second->username;
    }
    
    return userList;
}

// 发送在线用户列表
void SendUserList(SocketType socket) {
    std::string userList = GetOnlineUsers();
    Message msg(MSG_USER_LIST, "Server", userList);
    SendMessage(socket, msg);
}

// 移除客户端
void RemoveClient(SocketType socket) {
    std::lock_guard<std::mutex> lock(g_clientsMutex);
    
    auto it = g_clients.find(socket);
    if (it != g_clients.end()) {
        std::string username = it->second->username;
        
        // 关闭套接字
        closesocket(socket);
        
        // 从映射表中移除
        delete it->second;
        g_clients.erase(it);
        
        std::cout << "[" << GetCurrentTime() << "] User '" << username 
                 << "' disconnected. Total users: " << g_clients.size() << std::endl;
        
        // 通知其他客户端
        Message leaveMsg(MSG_USER_LEAVE, "Server", username);
        BroadcastMessage(leaveMsg);
    }
}

// 处理客户端连接
void HandleClient(ClientInfo* clientInfo) {
    SocketType clientSocket = clientInfo->socket;
    Message msg;
    
    // 接收登录消息
    if (!ReceiveMessage(clientSocket, msg) || msg.header.type != MSG_LOGIN) {
        std::cout << "[" << GetCurrentTime() << "] Invalid login request" << std::endl;
        closesocket(clientSocket);
        delete clientInfo;
        return;
    }
    
    std::string username = msg.sender;
    
    // 验证用户名
    if (!IsValidUsername(username)) {
        Message failMsg(MSG_LOGIN_FAIL, "Server", "Invalid username");
        SendMessage(clientSocket, failMsg);
        closesocket(clientSocket);
        delete clientInfo;
        return;
    }
    
    // 检查用户名是否已存在
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        for (auto& pair : g_clients) {
            if (pair.second->username == username) {
                Message failMsg(MSG_LOGIN_FAIL, "Server", "Username already exists");
                SendMessage(clientSocket, failMsg);
                closesocket(clientSocket);
                delete clientInfo;
                return;
            }
        }
    }
    
    // 设置用户名
    clientInfo->username = username;
    
    // 添加到客户端列表
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        g_clients[clientSocket] = clientInfo;
    }
    
    std::cout << "[" << GetCurrentTime() << "] User '" << username 
             << "' logged in. Total users: " << g_clients.size() << std::endl;
    
    // 发送登录成功消息
    Message successMsg(MSG_LOGIN_SUCCESS, "Server", "Login successful");
    SendMessage(clientSocket, successMsg);
    
    // 发送在线用户列表
    SendUserList(clientSocket);
    
    // 通知其他客户端有新用户加入
    Message joinMsg(MSG_USER_JOIN, "Server", username);
    BroadcastMessage(joinMsg, clientSocket);
    
    // 接收并处理客户端消息
    while (g_serverRunning) {
        Message recvMsg;
        if (!ReceiveMessage(clientSocket, recvMsg)) {
            // 连接断开
            break;
        }
        
        std::cout << "[" << GetCurrentTime() << "] Received from " 
                 << recvMsg.sender << ": " << recvMsg.content << std::endl;
        
        switch (recvMsg.header.type) {
            case MSG_CHAT:
                // 广播聊天消息给所有其他客户端
                BroadcastMessage(recvMsg, clientSocket);
                break;
                
            case MSG_LOGOUT:
                // 客户端主动退出
                std::cout << "[" << GetCurrentTime() << "] User '" << username 
                         << "' logged out" << std::endl;
                RemoveClient(clientSocket);
                return;
                
            default:
                std::cout << "[" << GetCurrentTime() << "] Unknown message type: " 
                         << recvMsg.header.type << std::endl;
                break;
        }
    }
    
    // 客户端异常断开
    RemoveClient(clientSocket);
}

int main(int argc, char* argv[]) {
    // 设置控制台UTF-8编码(Windows)
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif

    std::cout << "==================================" << std::endl;
    std::cout << "   Multi-User Chat Server" << std::endl;
    std::cout << "==================================" << std::endl;
    
    // 初始化网络库
    if (!InitNetwork()) {
        std::cerr << "Failed to initialize network" << std::endl;
        return 1;
    }
    
    // 获取端口号
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number" << std::endl;
            CleanupNetwork();
            return 1;
        }
    }
    
    // 创建套接字
    SocketType serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET_VALUE) {
        PrintError("Failed to create socket");
        CleanupNetwork();
        return 1;
    }
    
    // 设置地址重用
    int optval = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&optval), sizeof(optval));
    
    // 绑定地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR_VALUE) {
        PrintError("Failed to bind socket");
        closesocket(serverSocket);
        CleanupNetwork();
        return 1;
    }
    
    // 监听
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR_VALUE) {
        PrintError("Failed to listen on socket");
        closesocket(serverSocket);
        CleanupNetwork();
        return 1;
    }
    
    std::cout << "[" << GetCurrentTime() << "] Server started on port " << port << std::endl;
    std::cout << "Waiting for clients to connect..." << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << "----------------------------------" << std::endl;
    
    // 接受客户端连接
    while (g_serverRunning) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SocketType clientSocket = accept(serverSocket, 
                                         reinterpret_cast<sockaddr*>(&clientAddr), 
                                         &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET_VALUE) {
            PrintError("Failed to accept client");
            continue;
        }
        
        std::cout << "[" << GetCurrentTime() << "] New connection from " 
                 << inet_ntoa(clientAddr.sin_addr) << ":" 
                 << ntohs(clientAddr.sin_port) << std::endl;
        
        // 检查客户端数量限制
        if (g_clients.size() >= MAX_CLIENTS) {
            std::cout << "[" << GetCurrentTime() << "] Server is full, rejecting connection" << std::endl;
            Message rejectMsg(MSG_LOGIN_FAIL, "Server", "Server is full");
            SendMessage(clientSocket, rejectMsg);
            closesocket(clientSocket);
            continue;
        }
        
        // 创建客户端信息并启动处理线程
        ClientInfo* clientInfo = new ClientInfo(clientSocket, "");
        clientInfo->thread = std::thread(HandleClient, clientInfo);
        clientInfo->thread.detach();
    }
    
    // 清理资源
    std::cout << "\nShutting down server..." << std::endl;
    
    closesocket(serverSocket);
    
    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(g_clientsMutex);
        for (auto& pair : g_clients) {
            closesocket(pair.first);
            delete pair.second;
        }
        g_clients.clear();
    }
    
    CleanupNetwork();
    
    std::cout << "Server stopped" << std::endl;
    
    return 0;
}
