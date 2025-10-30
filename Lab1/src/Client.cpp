//这是一个客户端的简单程序
#include<iostream>
#include<winsock2.h>

int main(){
    //初始化阶段属于 Socket API 的系统级准备，相当于在用户空间注册本进程的网络会话句柄。
    WSADATA wsaData;
    int res=WSAStartup(MAKEWORD(2,2), &wsaData);
    if(res!=0){
        std::cout<<"Load WSA failed"<<std::endl;
        return 0;
    }

    //创建套接字并进行配置
    SOCKET clientSocket=socket(AF_INET,SOCK_STREAM,0);
    //AF_INET 指定使用 IPv4 地址族, SOCK_STREAM是使用流式套接字
    if(clientSocket==INVALID_SOCKET){
        std::cout<<"Create Socket failed"<<std::endl;
        WSACleanup();
        return 0;
    }
    //配置套接字
    sockaddr_in serverAddr{};
    serverAddr.sin_family=AF_INET;//地址簇为IPV4
    serverAddr.sin_port=htons(8888);//端口号转换为网络字节序
    serverAddr.sin_addr.s_addr=inet_addr("127.0.0.1");//本地回环地址

    /*该结构体属于“IPv4 套接字编址结构”，定义客户端目标的传输层标识符四元组中的远端信息：
    (目的IP，目的Port)。*/

    //连接到服务器
    if(connect(clientSocket,(sockaddr*)&serverAddr,sizeof(serverAddr))==SOCKET_ERROR){
        std::cout<<"Connect to Server failed"<<std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 0;
    }
    /*connect() 在内核态为该 socket 发起主动连接请求；
    调用成功标志 TCP 状态机由 CLOSED → SYN_SENT → ESTABLISHED 转换。*/
    const char* message="Hello Server,this is Client!";
    send(clientSocket,message ,strlen(message),0);

    char buffer[4096]={0};
    int byteReceived=recv(clientSocket,buffer,sizeof(buffer),0);
    if(byteReceived>0){
        std::cout<<"[Client]Received:"<<buffer<<std::endl;
    }
    closesocket(clientSocket);
    WSACleanup();

    return 0;   

}