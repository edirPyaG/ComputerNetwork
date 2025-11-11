#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>
#include<winsock2.h>
#include"../include/Common.h"
#include <mutex>

//定义接口和用户名之间的映射

std:: map<SOCKET,std::string> socketUser;
std::map<std::string, SOCKET> userSocket;
std::mutex clientMutex;//保护映射表的互斥锁




//主要函数声明
void handleClient(SOCKET clientSocket); //处理客户端请求
void onJoin(const Message& m, SOCKET clientSocket);
void onMsg(const Message& m, SOCKET clientSocket);
void onExit(const Message& m, SOCKET clientSocket);
//处理消息
void handleMessage(const Message &m, SOCKET clientSocket);
// 通用广播
void broadcast(const std::string& data, SOCKET excludeSocket = INVALID_SOCKET);

#endif // SERVER_H

