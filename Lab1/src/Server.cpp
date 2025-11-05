
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>
#include<winsock2.h>
#include"../include/Common.h"
#include"../include/Server.h"
//广播函数实现
void broadcast(const std::string & data, SOCKET excludeSocket){
    std::lock_guard<std::mutex> lock(clientMutex);//加锁保护映射表
    for(const auto &[name,socket]:userSocket){
        if(socket!=excludeSocket){
            send(socket,data.c_str(),data.size(),0);    
        }
    }
}
//处理用户消息,并且回显
void onJoin(const Message & m,SOCKET clientSocket){
    std::lock_guard<std::mutex> lock(clientMutex);//加锁保护映射表
    //添加新的映射入表
    userSocket[m.sender]=clientSocket;
    socketUser[clientSocket]=m.sender;

    Message joinMsg{"Sys","Server","All",m.sender+"has joined the chat."};
    std::string strMsg=buildMessage(joinMsg);
    broadcast(strMsg, clientSocket);
    //在终端(服务器处输出提示)
    std::cout<<std::string("[JOIN]"+m.sender)<<std::endl;
}

void onExit(const Message&m ,SOCKET clientSocket){
    std::lock_guard<std::mutex>lock(clientMutex);//加锁保护映射表
    //删除对应的映射表
    userSocket.erase(m.sender);
    socketUser.erase(clientSocket);

    Message exitMsg{"Sys","Server","All",m.sender+"has left the chat."};
    std::string strMsg=buildMessage(exitMsg);
    broadcast(strMsg,clientSocket);
    //在终端(服务器处输出提示)
    std::cout<<std::string ("[EXIT]"+m.sender)<<std::endl;
}

void onMsg(const Message & m,SOCKET clientSocket){
    std::lock_guard<std::mutex> lock(clientMutex);//加锁保护映射表
    //回显消息
    std::string strMsg=buildMessage(m);
    //处理不同类型的消息
    if(m.accepter=="All"){
        broadcast(strMsg,clientSocket);
    }
    else{
        //从映射表找到对应的套接字并发送
        SOCKET accepterSocket=userSocket[m.accepter];
        send(accepterSocket,strMsg.c_str(),strMsg.size(),0);
    }
    //在终端(服务器处输出提示)
    std::cout<<std::string("[MSG] From "+m.sender+" to "+m.accepter+": "+m.content)<<std::endl;
}
//处理Client消息的函数
void handleMessage(const Message &m, SOCKET clientSocket){
    if (m.type == "JOIN")      onJoin(m, clientSocket);
    else if (m.type == "MSG")  onMsg(m, clientSocket);
    else if (m.type == "EXIT") onExit(m, clientSocket);
    else {
        std::cout << "[WARN] Unknown message type: " << m.type << std::endl;
    }
}
//定义处理Client消息的函数
void handleClient(SOCKET clientSocket){
    char buffer[4096];
    //修改接受信息逻辑,实现多次通信
    while (true) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        /*recv() 是应用层与传输层的边界操作，取出 TCP 接收窗口内的数据段。数据可能被拆包/粘包，因此应用层需自行定义消息边界（后续协议设计要考虑）。*/
        std::string msg(buffer);
        Message m = parseMessage(msg);
        if (m.type == "EXIT") {
            onExit(m, clientSocket);
            break;
        }
    }
    closesocket(clientSocket);//先关闭客户端套接字
}

int main(){
    //初始化阶段属于 Socket API 的系统级准备
    WSADATA wsaData;
    int res=WSAStartup(MAKEWORD(2,2),&wsaData);
    if(res!=0){
        std::cout<<"Load WSA failed"<<std::endl;
        return 1;
    }
    SOCKET serverSocket=socket(AF_INET,SOCK_STREAM,0);
    if(serverSocket==INVALID_SOCKET){
        std::cout<<"Create Socket failed"<<std::endl;
        return 1;
        //验证套接字是否创建成功
    }
    //绑定套接字
    sockaddr_in serverAddr{};
    serverAddr.sin_family=AF_INET;//地址簇为ipv4
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8888);
    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    /*该调用与 TCP 四元组中的“源地址与源端口”关联，确立服务器的传输层标识符(源IP, 源Port, ANY目标IP, ANY目标Port)。系统随后能根据目的端口匹配到此监器。*/
    //监听
    listen(serverSocket,SOMAXCONN); //开始监听传入连接请求
    std::cout<<"Server is listening on port 8888..."<<std::endl;
    //接受消息
    while(true){
        //接受Client的链接
        std::cout<<"Waiting for client connection..."<<std::endl;
        sockaddr_in clientAddr{};
        int clientAddrLen=sizeof(clientAddr);
        SOCKET clientSocket=accept(serverSocket,(sockaddr*)&clientAddr,&clientAddrLen);
        if(clientSocket==INVALID_SOCKET){
            std::cout<<"Accept failed"<<std::endl;
        }
        //建立连接后多线程处理消息
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();  // 分离线程
    }
    closesocket(serverSocket);//后关闭服务器套接字
    WSACleanup();//关闭windows socket环境
    return 0;
}