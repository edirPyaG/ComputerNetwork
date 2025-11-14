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
#include <ctime>
#include <winsock2.h>
#include "../include/Client.h"     // （预留接口）客户端类或辅助定义
#include "../include/Storage.h"    // Storage 数据库类
#pragma comment(lib, "ws2_32.lib")

// 全局变量定义
std::map<std::string, ClientSession> sessions;
std::string currSessionId;
std::string currUserName;
Storage* storage = nullptr;  // 全局数据库对象

// ========== 时间戳格式化工具函数实现 ==========

// 格式化时间戳为易读字符串
std::string formatTimestamp(int64_t timestamp) {
    if (timestamp == 0) {
        return "";
    }
    
    time_t rawTime = static_cast<time_t>(timestamp);
    time_t now = std::time(nullptr);
    
    struct tm* timeInfo = localtime(&rawTime);
    struct tm* nowInfo = localtime(&now);
    
    char buffer[100];
    
    // 计算天数差
    int dayDiff = nowInfo->tm_yday - timeInfo->tm_yday;
    
    if (dayDiff == 0 && nowInfo->tm_year == timeInfo->tm_year) {
        // 今天：只显示时:分
        strftime(buffer, sizeof(buffer), "%H:%M", timeInfo);
    } else if (dayDiff == 1 && nowInfo->tm_year == timeInfo->tm_year) {
        // 昨天：显示"昨天 时:分"
        strftime(buffer, sizeof(buffer), "昨天 %H:%M", timeInfo);
    } else if (dayDiff < 7 && nowInfo->tm_year == timeInfo->tm_year) {
        // 一周内：显示"星期X 时:分"
        const char* weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        char timeStr[50];
        strftime(timeStr, sizeof(timeStr), "%H:%M", timeInfo);
        snprintf(buffer, sizeof(buffer), "%s %s", weekdays[timeInfo->tm_wday], timeStr);
    } else if (nowInfo->tm_year == timeInfo->tm_year) {
        // 今年：显示"月-日 时:分"
        strftime(buffer, sizeof(buffer), "%m-%d %H:%M", timeInfo);
    } else {
        // 跨年：显示"年-月-日 时:分"
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeInfo);
    }
    
    return std::string(buffer);
}

// 判断是否需要显示时间戳
bool shouldShowTimestamp(int64_t currentTime, int64_t lastTime, int threshold) {
    // 如果是第一条消息，显示时间戳
    if (lastTime == 0) {
        return true;
    }
    
    // 如果时间间隔超过阈值（默认5分钟），显示时间戳
    int64_t diff = currentTime - lastTime;
    return (diff >= threshold);
}

// ==========================================================================
// 线程函数：发送线程
// 职责：负责读取用户输入、封装协议消息并发送至服务器。
// ==========================================================================
void sendThread(SOCKET clientSocket, const std::string &userName) {

    // --------------------- 1. 用户登录阶段 ---------------------
    // 首次连接后，发送 "JOIN" 协议消息，仅注册用户名（不加入任何session）
    Message joinMsg{"JOIN", userName, "", ""};       // accepter 为空
    std::string data = buildMessage(joinMsg);
    send(clientSocket, data.c_str(), (int)data.size(), 0);
    
    std::cout << "\n[提示] 请使用 /join <会话名> 加入会话" << std::endl;
    std::cout << "[提示] 例如：/join ALL 加入聊天室\n" << std::endl;

    // --------------------- 2. 聊天循环阶段 ---------------------
    while (true) {
        std::string input;
        std::cout << userName << " [" << (currSessionId.empty() ? "未加入" : currSessionId) << "]: ";
        std::getline(std::cin, input);

        // 跳过空输入，防止输入回车直接发送空字符串导致阻塞
        if (input.empty()) continue;
        
        // 处理命令
        if (input[0] == '/') {
            // 去除开头的斜杠，并去除前后空格
            std::string command = input.substr(1);
            // 去除前导空格
            size_t start = command.find_first_not_of(" \t");
            if (start != std::string::npos) {
                command = command.substr(start);
            }
            
            if (command == "exit") {
                // 组装 EXIT 协议包并发送
                Message exitMsg{"EXIT", userName, "", ""};
                std::string exitData = buildMessage(exitMsg);
                send(clientSocket, exitData.c_str(), (int)exitData.size(), 0);
                std::cout << "[Client] Exiting...\n";
                break;
            }
            else if (command.substr(0, 4) == "join") {
                // 加入会话
                // 查找第一个空格后的内容
                size_t spacePos = command.find(' ');
                if (spacePos == std::string::npos || spacePos + 1 >= command.length()) {
                    std::cout << "[错误] 用法: /join <会话名>" << std::endl;
                    continue;
                }
                std::string targetSession = command.substr(spacePos + 1);
                // 去除目标会话名的前后空格
                size_t sessStart = targetSession.find_first_not_of(" \t");
                size_t sessEnd = targetSession.find_last_not_of(" \t");
                if (sessStart != std::string::npos) {
                    targetSession = targetSession.substr(sessStart, sessEnd - sessStart + 1);
                }
                
                std::cout << "[DEBUG] Joining session: [" << targetSession << "]" << std::endl;
                
                Message joinSessionMsg{"JOIN_SESSION", userName, targetSession, ""};
                std::string joinData = buildMessage(joinSessionMsg);
                send(clientSocket, joinData.c_str(), (int)joinData.size(), 0);
                
                // 本地创建 session（如果不存在）
                if (sessions.find(targetSession) == sessions.end()) {
                    ClientSession newSession;
                    newSession.id = targetSession;
                    newSession.type = (targetSession == "ALL") ? ST_GROUP : ST_PRIVATE;
                    sessions[targetSession] = newSession;
                }
                
                // 切换到该 session
                currSessionId = targetSession;
                std::cout << "[Client] 正在加入会话: " << targetSession << std::endl;
                continue;
            }
            else if (command.substr(0, 5) == "leave") {
                // 离开会话
                size_t spacePos = command.find(' ');
                if (spacePos == std::string::npos || spacePos + 1 >= command.length()) {
                    std::cout << "[错误] 用法: /leave <会话名>" << std::endl;
                    continue;
                }
                std::string targetSession = command.substr(spacePos + 1);
                // 去除前后空格
                size_t sessStart = targetSession.find_first_not_of(" \t");
                size_t sessEnd = targetSession.find_last_not_of(" \t");
                if (sessStart != std::string::npos) {
                    targetSession = targetSession.substr(sessStart, sessEnd - sessStart + 1);
                }
                
                Message leaveSessionMsg{"LEAVE_SESSION", userName, targetSession, ""};
                std::string leaveData = buildMessage(leaveSessionMsg);
                send(clientSocket, leaveData.c_str(), (int)leaveData.size(), 0);
                
                // 如果离开的是当前会话，清空 currSessionId
                if (currSessionId == targetSession) {
                    currSessionId = "";
                }
                
                std::cout << "[Client] 正在离开会话: " << targetSession << std::endl;
                continue;
            }
            else if (command.substr(0, 6) == "switch") {
                // 切换会话（不需要发送到服务器，纯本地操作）
                size_t spacePos = command.find(' ');
                if (spacePos == std::string::npos || spacePos + 1 >= command.length()) {
                    std::cout << "[错误] 用法: /switch <会话名>" << std::endl;
                    continue;
                }
                std::string targetSession = command.substr(spacePos + 1);
                // 去除前后空格
                size_t sessStart = targetSession.find_first_not_of(" \t");
                size_t sessEnd = targetSession.find_last_not_of(" \t");
                if (sessStart != std::string::npos) {
                    targetSession = targetSession.substr(sessStart, sessEnd - sessStart + 1);
                }
                
                // 检查是否已加入该 session
                if (sessions.find(targetSession) == sessions.end()) {
                    std::cout << "[错误] 你还没有加入会话 " << targetSession << std::endl;
                    std::cout << "[提示] 请先 /join " << targetSession << std::endl;
                    continue;
                }
                
                currSessionId = targetSession;
                std::cout << "[Client] 已切换到会话: " << currSessionId << std::endl;
                
                // 显示最近的消息
                auto &history = sessions[currSessionId].history;
                int startIdx = (history.size() > 5) ? (int)(history.size() - 5) : 0;
                if (startIdx < (int)history.size()) {
                    std::cout << "--- 最近消息 ---" << std::endl;
                    
                    int64_t lastTime = 0;
                    for (size_t i = startIdx; i < history.size(); i++) {
                        // 智能显示时间戳
                        if (shouldShowTimestamp(history[i].timestamp, lastTime, 300)) {
                            std::cout << "--- " << formatTimestamp(history[i].timestamp) << " ---" << std::endl;
                        }
                        
                        std::cout << "[" << history[i].sender << "] " << history[i].content << std::endl;
                        lastTime = history[i].timestamp;
                    }
                    std::cout << "---------------" << std::endl;
                }
                
                continue;
            }
            else if (command == "sessions") {
                // 显示所有会话
                std::cout << "\n=== 我的会话列表 ===" << std::endl;
                for (const auto &[sid, session] : sessions) {
                    std::string typeStr = (session.type == ST_GROUP) ? "群聊" : "私聊";
                    std::string current = (sid == currSessionId) ? " [当前]" : "";
                    int unreadCount = 0;
                    if (storage) {
                        unreadCount = storage->getUnreadCount(sid);
                    }
                    std::cout << sid << " [" << typeStr << "] - " 
                              << session.history.size() << " 条消息" << current;
                    if (unreadCount > 0) {
                        std::cout << " (未读: " << unreadCount << ")";
                    }
                    std::cout << std::endl;
                }
                std::cout << "===================\n" << std::endl;
                continue;
            }
            else if (command == "history") {
                // 查看当前会话的历史记录
                if (currSessionId.empty()) {
                    std::cout << "[错误] 请先加入一个会话" << std::endl;
                    continue;
                }
                
                if (storage) {
                    // 从数据库加载完整历史
                    auto fullHistory = storage->loadHistory(currSessionId, 100);
                    
                    std::cout << "\n========== " << currSessionId << " 聊天记录 ==========" << std::endl;
                    std::cout << "（共 " << fullHistory.size() << " 条）" << std::endl;
                    std::cout << "-------------------------------------------" << std::endl;
                    
                    int64_t lastTime = 0;
                    for (const auto& msg : fullHistory) {
                        // 智能显示时间戳
                        if (shouldShowTimestamp(msg.timestamp, lastTime, 300)) {
                            std::cout << "\n--- " << formatTimestamp(msg.timestamp) << " ---" << std::endl;
                        }
                        
                        std::cout << "[" << msg.sender << "] " << msg.content << std::endl;
                        lastTime = msg.timestamp;
                    }
                    
                    std::cout << "==========================================\n" << std::endl;
                } else {
                    std::cout << "[错误] 数据库未初始化" << std::endl;
                }
                continue;
            }
            else {
                std::cout << "[错误] 未知命令。可用命令：" << std::endl;
                std::cout << "  /join <会话名>   - 加入会话" << std::endl;
                std::cout << "  /leave <会话名>  - 离开会话" << std::endl;
                std::cout << "  /switch <会话名> - 切换会话" << std::endl;
                std::cout << "  /sessions        - 显示所有会话" << std::endl;
                std::cout << "  /history         - 查看当前会话历史" << std::endl;
                std::cout << "  /exit            - 退出程序" << std::endl;
                continue;
            }
        }
        
        // 检查是否在某个 session 中
        if (currSessionId.empty()) {
            std::cout << "[错误] 请先加入一个会话，例如：/join ALL" << std::endl;
            continue;
        }
        
        // 发送消息到当前 session
        Message msg{"MSG", userName, currSessionId, input};
        std::string sendData = buildMessage(msg);
        send(clientSocket, sendData.c_str(), (int)sendData.size(), 0);
        
        //  保存到数据库
        if (storage) {
            SessionType type = sessions[currSessionId].type;
            if (storage->saveMessage(msg, currSessionId, type)) {
                // 更新本地会话的最后时间
                sessions[currSessionId].lastReadTime = time(nullptr);
            }
        }
        
        // 本地内存也保存（发送的消息也要记录）
        sessions[currSessionId].history.push_back(msg);
    }

    // --------------------- 3. 退出资源阶段 ---------------------
}

// ==========================================================================
// 线程函数：接收线程
// 职责：持续监听服务器的消息回传，并在本地解析、输出。
// ==========================================================================
void recvThread(SOCKET clientSocket) {
    char buffer[4096];
    while (true) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            std::cout << "\n[Client] 连接已断开" << std::endl;
            break;
        }

        buffer[bytes] = '\0';
        Message m = parseMessage(std::string(buffer));

        // 根据消息类型进行分类处理
        if (m.type == "SYS") {
            // 系统消息（总是显示）
            std::cout << "\n[系统] " << m.content << std::endl;
        }
        else if (m.type == "NOTIFY") {
            // 通知消息（如新私聊）
            std::cout << "\n[通知] " << m.content << std::endl;
        }
        else if (m.type == "MSG") {
            // 普通消息
            std::string msgSessionId;
            
            // 判断消息属于哪个 session
            if (m.accepter == "ALL") {
                msgSessionId = "ALL";
            } else if (m.sender == currUserName) {
                // 我发的消息（回显）
                msgSessionId = m.accepter;
            } else {
                // 别人发给我的消息
                msgSessionId = m.sender;
            }
            
            // 保存到对应 session
            if (sessions.find(msgSessionId) == sessions.end()) {
                // 自动创建 session
                ClientSession newSession;
                newSession.id = msgSessionId;
                newSession.type = (msgSessionId == "ALL") ? ST_GROUP : ST_PRIVATE;
                newSession.lastReadTime = 0;
                sessions[msgSessionId] = newSession;
                
                // 保存会话到数据库
                if (storage) {
                    storage->saveSession(msgSessionId, newSession.type);
                }
            }
            
            // 保存到内存
            sessions[msgSessionId].history.push_back(m);
            
            // 保存到数据库
            if (storage) {
                SessionType type = sessions[msgSessionId].type;
                storage->saveMessage(m, msgSessionId, type);
                
                // 如果是当前会话，更新已读时间
                if (msgSessionId == currSessionId) {
                    sessions[msgSessionId].lastReadTime = time(nullptr);
                    storage->updateLastSyncTime(msgSessionId, time(nullptr));
                }
            }
            
            // 只显示当前 session 的消息
            if (msgSessionId == currSessionId) {
                // 智能显示时间戳
                int64_t lastMsgTime = 0;
                
                // 获取上一条消息的时间戳
                auto &history = sessions[msgSessionId].history;
                if (history.size() > 1) {
                    // history 最后一个是刚刚添加的当前消息，倒数第二个是上一条
                    lastMsgTime = history[history.size() - 2].timestamp;
                }
                
                // 判断是否需要显示时间戳（默认阈值：5分钟 = 300秒）
                if (shouldShowTimestamp(m.timestamp, lastMsgTime, 300)) {
                    std::string timeStr = formatTimestamp(m.timestamp);
                    std::cout << "\n--- " << timeStr << " ---" << std::endl;
                }
                
                std::cout << "[" << m.sender << "] " << m.content << std::endl;
            } else {
                // 其他 session 有新消息，提示
                std::cout << "\n[新消息 @" << msgSessionId << "] " 
                          << m.sender << ": " << m.content << std::endl;
            }
        }

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
    std::cout << "请输入昵称(Please enter your username): ";
    std::getline(std::cin, username);
    currUserName = username;  // 设置全局用户名变量
    
    // 初始化数据库
    storage = new Storage(username);
    if (!storage->init()) {
        std::cout << "[ERROR] 数据库初始化失败！" << std::endl;
        delete storage;
        storage = nullptr;
        closesocket(clientSocket);
        WSACleanup();
        return 0;
    }
    
    std::cout << "\n[SYS] 数据库已加载" << std::endl;
    
    //  从数据库恢复历史会话
    auto sessionList = storage->loadSessions();
    if (sessionList.size() > 0) {
        std::cout << "[SYS] 找到 " << sessionList.size() << " 个历史会话：" << std::endl;
        
        for (const auto& sid : sessionList) {
            ClientSession sess;
            sess.id = sid;
            sess.type = storage->getSessionType(sid);
            
            // 加载最近 10 条消息作为预览
            sess.history = storage->loadHistory(sid, 10);
            
            // 记录最后同步时间
            sess.lastReadTime = storage->getLastMessageTime(sid);
            
            sessions[sid] = sess;
            
            std::string typeStr = (sess.type == ST_GROUP) ? "群聊" : "私聊";
            std::cout << "  - " << sid << " [" << typeStr << "] " 
                      << "(" << sess.history.size() << " 条历史)" << std::endl;
        }
        std::cout << "[提示] 使用 /switch <会话名> 切换到历史会话" << std::endl;
    } else {
        std::cout << "[SYS] 这是你首次使用，开始新的聊天吧！" << std::endl;
    }
    
    // 不自动切换到任何 session，用户需要主动 /join
    currSessionId = "";  // 空字符串表示未加入任何 session

    // 使用两个独立线程同时发送和接收数据，实现双向通信
    std::thread sender(sendThread, clientSocket, username);   // 处理键盘输入与发送
    std::thread receiver(recvThread, clientSocket);       // 处理服务器广播接收

    // --------------------- 第六阶段：等待线程自然结束 ---------------------
    // 主线程等待子线程执行完毕，防止程序过早退出
    sender.join();
    shutdown(clientSocket, SD_BOTH); // 通知服务器结束发送接收
    closesocket(clientSocket);      // 真正关闭
    receiver.join();

    // 清理数据库资源
    if (storage) {
        storage->close();
        delete storage;
        storage = nullptr;
    }

    // --------------------- 第七阶段：释放网络资源 ---------------------
    // 所有通信结束后，通过 WSACleanup() 释放 Winsock 资源。
    WSACleanup();

    return 0;   // 程序正常结束
}