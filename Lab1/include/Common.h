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

<<<<<<< HEAD
//定义消息类型
enum TYPE{SYS, JOIN , MSG ,EXIT, SWITCH_CHAT};
=======
>>>>>>> d75a0fb4afe60d6c9a761753aedec4a22577c70c
#endif // COMMON_H