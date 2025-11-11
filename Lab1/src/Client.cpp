// ===================== 实验：多线程协议化聊天室客户端 =====================
// 功能说明：
// 1. 使用原生 Winsock 完成与服务器的 TCP 通信。
// 2. 实现 “TYPE|SENDER|ACCEPTER|MESSAGE” 自定义协议。
// 3. 使用两个线程实现全双工通信（同时发送与接收）。
// 4. 支持系统消息、群聊消息、正常退出。
// ==========================================================================

#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include "../include/Common.h"     // 定义 Message 结构体与解析/封装函数
#include "../include/Client.h"     // （预留接口）客户端类或辅助定义
#pragma comment(lib, "ws2_32.lib")

// ==========================================================================
// 线程函数：发送线程
// 职责：负责读取用户输入、封装协议消息并发送至服务器。
// ==========================================================================
void sendThread(SOCKET clientSocket, const std::string &userName) {

    // --------------------- 1. 用户登录阶段 ---------------------
    // 首次连接后，发送 "JOIN" 协议消息，告诉服务器自己的昵称
    Message joinMsg{"JOIN", userName, "ALL", ""};       // TYPE|SENDER|ACCEPTER|MESSAGE
    std::string data = buildMessage(joinMsg);            // 封装为字符串
    send(clientSocket, data.c_str(), (int)data.size(), 0);

    // --------------------- 2. 聊天循环阶段 ---------------------
    while (true) {
        std::string input;
        std::cout << userName << ": ";
        std::getline(std::cin, input);

        // 跳过空输入，防止输入回车直接发送空字符串导致阻塞
        if (input.empty()) continue;

        // 处理退出命令（由用户在客户端主动输入 "/exit"）
        if (input == "/exit") {
            // 组装 EXIT 协议包并发送
            Message exitMsg{"EXIT", userName, "ALL", ""};
            std::string exitData = buildMessage(exitMsg);
            send(clientSocket, exitData.c_str(), (int)exitData.size(), 0);
            std::cout << "[Client] Exiting...\n";
            break; // 跳出循环，线程结束
        }

        // 否则认为是普通群发聊天消息，类型为 MSG
        Message msg{"MSG", userName, "ALL", input};
        std::string sendData = buildMessage(msg);
        send(clientSocket, sendData.c_str(), (int)sendData.size(), 0);
    }

    // --------------------- 3. 退出资源阶段 ---------------------
    // 主动关闭自身 socket，通知服务器断开连接。
    // 接收线程会因为 recv() 出错或返回 0 自动结束。
    //放在主函数中执行
}

// ==========================================================================
// 线程函数：接收线程
// 职责：持续监听服务器的消息回传，并在本地解析、输出。
// ==========================================================================
void recvThread(SOCKET clientSocket) {
    char buffer[4096]; // 接收缓冲区
    while (true) {
        // recv() 阻塞式接收服务器端数据
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) { // 若返回 <=0，说明服务器关闭或发生错误
            std::cout << "\n[Client] Server closed connection.\n";
            break;
        }

        buffer[bytes] = '\0';  // 添加字符串终止符
        std::string raw(buffer);

        // 对收到的数据进行协议解析，转换为结构体 Message
        Message m = parseMessage(raw);

        // 根据消息类型进行分类处理
        if (m.type == "SYS") {               // 系统广播消息
            std::cout << "\n[系统] " << m.content << std::endl;
        } else if (m.type == "MSG") {        // 普通聊天消息
            std::cout << "\n[" << m.sender << "] " << m.content << std::endl;
        }

        // 打印完内容后重新输出提示符，保持良好交互体验。
        std::cout << "You: ";
        std::flush(std::cout);
    }
}

// ==========================================================================
// 函数：main()
// 职责：负责客户端主逻辑，创建 socket、连接服务器、
//       启动发送与接收线程，实现全双工通信。
// ==========================================================================
int main() {
    //设置控制台支持中文
    SetConsoleOutputCP(65001); // 设置控制台输出为 UTF-8 编码
    SetConsoleCP(65001);

    // --------------------- 第一阶段：初始化 Winsock ---------------------
    // 初始化阶段属于 Socket API 的系统级准备，相当于在用户空间注册本进程的网络会话句柄。
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cout << "Load WSA failed" << std::endl;
        return 0;
    }

    // --------------------- 第二阶段：创建套接字 ---------------------
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    // AF_INET 指定使用 IPv4 地址族, SOCK_STREAM 表示使用 TCP 流式套接字
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "Create Socket failed" << std::endl;
        WSACleanup();
        return 0;
    }

    // --------------------- 第三阶段：配置目标服务器地址信息 ---------------------
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;                // 地址簇为 IPV4
    serverAddr.sin_port = htons(8888);              // 端口号需转换为网络字节序
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 本地回环地址
    /* 该结构体属于“IPv4 套接字编址结构”，定义客户端目标的传输层标识符四元组中的远端信息：
       (目的IP，目的Port)。*/

    // --------------------- 第四阶段：尝试主动建立与服务器的连接 ---------------------
    if (connect(clientSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Connect to Server failed" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 0;
    }
    /* connect() 在内核态为该 socket 发起主动连接请求；
       调用成功标志 TCP 状态机由 CLOSED → SYN_SENT → ESTABLISHED 转换。*/

    // --------------------- 第五阶段：启动通信线程 ---------------------
    std::string username;
    std::cout << "请输入昵称: ";          // 启动前要求用户输入昵称
    std::getline(std::cin, username);

    // 使用两个独立线程同时发送和接收数据，实现双向通信
    std::thread sender(sendThread, clientSocket, username);   // 处理键盘输入与发送
    std::thread receiver(recvThread, clientSocket);       // 处理服务器广播接收

    // --------------------- 第六阶段：等待线程自然结束 ---------------------
    // 主线程等待子线程执行完毕，防止程序过早退出
    sender.join();
    shutdown(clientSocket, SD_BOTH); // 通知服务器结束发送接收
    closesocket(clientSocket);      // 真正关闭
    receiver.join();

    // --------------------- 第七阶段：释放网络资源 ---------------------
    // 所有通信结束后，通过 WSACleanup() 释放 Winsock 资源。
    WSACleanup();

    return 0;   // 程序正常结束
}