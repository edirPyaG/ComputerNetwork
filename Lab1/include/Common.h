#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>
#include <winsock2.h>

struct Message {
    std::string type; // JOIN, MT_MSG, EXIT, SYS, etc.//将sessiontype和messagetype分离
    std::string sender;
    std::string accepter;
    std::string content;
};

//构造消息
std::string buildMessage(const Message& m) ;

//解析消息
Message parseMessage(const std::string& strMsg) ;

//修改枚举类型
enum MessageType{MT_SYS, MT_JOIN, MT_MSG, MT_EXIT, MT_SWITCH_CHAT};

enum SessionType{ST_GROUP, ST_PRIVATE, ST_SYSTEM};

#endif // COMMON_H