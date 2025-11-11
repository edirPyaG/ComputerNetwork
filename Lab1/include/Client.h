#ifndef CLIENT_H
#define CLIENT_H

#include<iostream>
#include<string>
#include<winsock2.h>

//发送消息线程函数声明
void sendThread(SOCKET clientSocket, const std::string& userName);

//接收消息线程函数声明
void recvThread(SOCKET clientSocket);

#endif // CLIENT_H
