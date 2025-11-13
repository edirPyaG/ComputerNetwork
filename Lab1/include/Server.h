#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>
#include<winsock2.h>
#include"Common.h"
#include <mutex>
#include<set>


//定义接口和用户名之间的映射
//管理所有在线的sessions
class ServerSession {
public:
    std::string id;
    SessionType type;
    std::set<std::string> members;  // 在线成员
};
std:: map<SOCKET,std::string> socketUser;
std::map<std::string, SOCKET> userSocket;
std::mutex clientMutex;//保护映射表的互斥锁,用于枷锁保护的参数
std::map<std::string, ServerSession> sessions;//用于管理所有的会话


//主要函数声明
void handleClient(SOCKET clientSocket); //处理客户端请求
void onJoin(const Message& m, SOCKET clientSocket);
void onMsg(const Message& m, SOCKET clientSocket);
void onExit(const Message& m, SOCKET clientSocket);
//处理消息
void handleMessage(const Message &m, SOCKET clientSocket);
// 通用广播
void broadcast(const std::string& data, SOCKET excludeSocket = INVALID_SOCKET);


//在服务器端增加会话管理函数
void createGroupSession(const std::string &groupName);
void createPrivateSession(const std::string &user1, const std::string &user2);
void addUserToSession(const std::string &sessionid ,const std::string & uerName);
void removeUserFromSession(const std::string &sessionId, const std::string & userName);
void broadcastToSession(const std::string & sessionId, const std::string & msg, SOCKET excludeSocket);

#endif // SERVER_H

