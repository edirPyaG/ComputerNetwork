#ifndef CLIENT_H
#define CLIENT_H

#include<iostream>
#include<string>
#include<winsock2.h>
#include<vector>
#include<map>
#include "Common.h" //包含公共头文件
//新增session的抽象
class ClientSession{
public:
    std::string id;
    SessionType type;
    std::vector<Message> history; //本地历史, 聊天记录的历史存储与客户端
    int64_t lastReadTime;  //最后同步消息时间, 用于进行客户端之间的消息同步

};
//用于定位对应的session
extern std::map<std::string, ClientSession> sessions;
extern std::string currSessionId;

extern std::string currUserName; //用于存储当前用户名的全局变量

//发送消息线程函数声明
void sendThread(SOCKET clientSocket, const std::string& userName);

//接收消息线程函数声明
void recvThread(SOCKET clientSocket);

// ========== 时间戳格式化工具函数 ==========

// 将时间戳格式化为字符串（如：14:30 或 昨天 14:30 或 2024-11-14 14:30）
std::string formatTimestamp(int64_t timestamp);

// 判断是否需要显示时间戳（根据上一条消息的时间间隔）
// 参数：当前消息时间戳，上一条消息时间戳，阈值（秒）
bool shouldShowTimestamp(int64_t currentTime, int64_t lastTime, int threshold = 300);

#endif // CLIENT_H
