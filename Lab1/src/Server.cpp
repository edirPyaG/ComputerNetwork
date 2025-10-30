
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>
#include<winsock2.h>

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
    sockaddr_in clientAddr{};
    int clientAddrLen=sizeof(clientAddr);
    SOCKET clientSocket=accept(serverSocket,(sockaddr*)&clientAddr,&clientAddrLen);
    if(clientSocket==INVALID_SOCKET){
        std::cout<<"Accept failed"<<std::endl;
    }

    char buffer[4096];
    int byteRecieve=recv(clientSocket, buffer,sizeof(buffer)-1,0);
    /*recv() 是应用层与传输层的边界操作，取出 TCP 接收窗口内的数据段。数据可能被拆包/粘包，因此应用层需自行定义消息边界（后续协议设计要考虑）。*/
    const char* reply = "Hello Client, this is Server!";
    send(clientSocket, reply, strlen(reply), 0); //发送消息
    closesocket(clientSocket);//先关闭客户端套接字
    closesocket(serverSocket);//后关闭服务器套接字
    WSACleanup();//关闭windows socket环境
    return 0;
}