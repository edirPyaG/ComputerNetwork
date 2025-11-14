#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>
#include <winsock2.h>
#include <ctime>

struct Message {
    std::string type; // JOIN, MT_MSG, EXIT, SYS, etc.//将sessiontype和messagetype分离
    std::string sender;
    std::string accepter;
    std::string content;
    int64_t timestamp;  // 消息时间戳（秒级Unix时间）
    
    // 默认构造函数
    Message() : timestamp(0) {}
    
    // 带参数的构造函数（兼容旧代码）
    Message(const std::string& t, const std::string& s, const std::string& a, const std::string& c)
        : type(t), sender(s), accepter(a), content(c), timestamp(std::time(nullptr)) {}
};

//构造消息
std::string buildMessage(const Message& m) ;

//解析消息
Message parseMessage(const std::string& strMsg) ;

//修改枚举类型
enum MessageType{
    MT_SYS,           // 系统消息
    MT_JOIN,          // 用户连接服务器（不加入任何session）
    MT_MSG,           // 普通消息
    MT_EXIT,          // 退出
    MT_JOIN_SESSION,  // 加入会话
    MT_LEAVE_SESSION, // 离开会话
    MT_NOTIFY         // 通知消息（如新私聊）
};

enum SessionType{ST_GROUP, ST_PRIVATE, ST_SYSTEM};

#endif // COMMON_H