#ifndef CLIENT_H
#define CLIENT_H

#include<iostream>
#include<string>
#include<winsock2.h>
#include<vector>
#include<map>
//新增session的抽象
class Session{
public:
    std::string id;
    TYPE type;
    std::vector<Message> history; 
};
//用于定位对应的session
extern std::map<std::string , Session> chatSession;
extern std::string currSessionId;


//发送消息线程函数声明
void sendThread(SOCKET clientSocket, const std::string& userName);

//接收消息线程函数声明
void recvThread(SOCKET clientSocket);

#endif // CLIENT_H
