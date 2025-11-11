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
    std::string type;
    std::string sender;
    std::string accepter;
    std::string content;
};

//构造消息
std::string buildMessage(const Message& m) ;

//解析消息
Message parseMessage(const std::string& strMsg) ;

#endif // COMMON_H