
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <algorithm>
#include<winsock2.h>
#include"../include/Common.h"
#include"../include/Server.h"
//完善session相关的函数
//创建群聊session函数
void createGroupSession(const std::string & groupName){
    std::lock_guard<std::mutex> lock(clientMutex);
    //C++ 的安全锁操作语句，它让多个线程在访问共享资源时保证互斥。
    if(sessions.find(groupName)==sessions.end()){
        Session newSession;
        newSession.id=groupName;
        sessions[groupName]=newSession;//将新建的会话添加到会话表中
    }
}
             
//创建私聊session
void createPrivateSessions(const std::string & user1,const std::string & user2){
    std::lock_guard<std::mutex> lock(clientMutex);
    std::string sessionID =user1< user2 ? user1+user2:user2+user1;
    if(sessions.find(sessionID)==sessions.end()){
        Session newSession;
        newSession.members.insert(user1);
        newSession.members.insert(user2);
        newSession.id=sessionID;
        sessions[sessionID]=newSession;
    }

}

//向session中添加用户
void addUserToSession(const std::string & sessionId ,const std::string &userName){
    std::lock_guard<std::mutex> lock(clientMutex);
    auto iter=sessions.find(sessionId);
    if(iter!=sessions.end()){
        //iter->second 取的是 map 项的值部分，也就是那个 Session 对象；
        iter->second.members.insert(userName);
    }
}

void removeUserFromSession(const std::string & sessionId,const std::string &userName){
    std::lock_guard<std::mutex> lock(clientMutex);
    auto iter=sessions.find(sessionId); //通过名字进行查找对应的session
    if(iter!=sessions.end()){
        iter->second.members.erase(userName);//删除session中的用户
    }
}

//向session内所有成员广播消息
void broadcastToSession(const std::string & sessionId, const std::string & msg, SOCKET excludeSocket){
    std::lock_guard<std::mutex> lock(clientMutex);
    auto iter =sessions.find(sessionId);
    
    //处理特殊的群组广播:all
    if(sessionId=="ALL"){
        for(const auto &[name,socket]:userSocket){
            if(socket!=excludeSocket){
                send(socket,msg.c_str(),msg.size(),0);
            }
        }
        return ;//直接返回，不再进行后续操作
    }

    if(iter!=sessions.end()){
        for(const auto &member:iter->second.members){
            auto iter2=userSocket.find(member);
            if(iter2!=userSocket.end()){
                send(iter2->second,msg.c_str(),msg.size(),0);
            }
        }
    }

}

//广播函数实现
void broadcast(const std::string & data, SOCKET excludeSocket){
    std::lock_guard<std::mutex> lock(clientMutex);//加锁保护映射表
    for(const auto &[name,socket]:userSocket){
        if(socket!=excludeSocket){
            int result = send(socket,data.c_str(),data.size(),0);
            if (result == SOCKET_ERROR) {
                std::cout << "[ERROR] broadcast send failed for user " << name << ", error: " << WSAGetLastError() << std::endl;
            } else {
                std::cout << "[SYS] broadcast sent " << result << " bytes to " << name << std::endl;
            }
        }
    }
}
//处理用户消息,并且回显
void onJoin(const Message & m,SOCKET clientSocket){
    std::cout << "[SYS] onJoin called for user: " << m.sender << std::endl;
    
    // 先给当前客户端发送欢迎消息（在加锁之前）
    Message welcomeMsg{"SYS","Server","ALL","欢迎加入聊天室(Welcome to join the chat)！"};
    std::string welcomeStr = buildMessage(welcomeMsg);
    int result = send(clientSocket, welcomeStr.c_str(), welcomeStr.size(), 0);
    if (result == SOCKET_ERROR) {
        std::cout << "[ERROR] Failed to send welcome message, error: " << WSAGetLastError() << std::endl;
    } else {
        std::cout << "[SYS] Sent welcome message, " << result << " bytes" << std::endl;
    }
    
    {
        std::lock_guard<std::mutex> lock(clientMutex);//加锁保护映射表
        //添加新的映射入表
        userSocket[m.sender]=clientSocket;
        socketUser[clientSocket]=m.sender;
    } // 锁在这里释放

    // 再向其他客户端广播加入消息（在锁释放后调用 broadcast）
    Message joinMsg{"SYS","Server","ALL",m.sender + " has joined the chat."};
    std::string strMsg=buildMessage(joinMsg);
    std::cout << "[SYS] Broadcasting JOIN message: [" << strMsg << "]" << std::endl;
    broadcast(strMsg, clientSocket);
    //在终端(服务器处输出提示)
    std::cout<<std::string("[JOIN]"+m.sender)<<std::endl;
    std::cout << "[SYS] onJoin completed" << std::endl;
}

void onExit(const Message&m ,SOCKET clientSocket){
    {
        std::lock_guard<std::mutex>lock(clientMutex);//加锁保护映射表
        //删除对应的映射表
        userSocket.erase(m.sender);
        socketUser.erase(clientSocket);
    } // 锁在这里释放

    Message exitMsg{"SYS","Server","ALL",m.sender + " has left the chat."};
    std::string strMsg=buildMessage(exitMsg);
    broadcast(strMsg,clientSocket);
    //在终端(服务器处输出提示)
    std::cout<<std::string ("[EXIT]"+m.sender)<<std::endl;
}

void onMsg(const Message & m,SOCKET clientSocket){
    std::string strMsg=buildMessage(m);
    // 查找目标 session
    auto it = sessions.find(m.accepter);
    if (it == sessions.end() && m.accepter != "ALL") {
        // 私聊：自动创建私聊 session
        createPrivateSession(m.sender, m.accepter);
    }
    
    // 向 session 内所有成员转发消息
    broadcastToSession(m.accepter, buildMessage(m), clientSocket);
    //处理不同类型的消息
    if(m.accepter=="All" || m.accepter=="ALL"){
        broadcast(strMsg,clientSocket);
    }
    else{
        //从映射表找到对应的套接字并发送
        SOCKET accepterSocket = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> lock(clientMutex);//加锁保护映射表
            auto it = userSocket.find(m.accepter);
            if (it != userSocket.end()) {
                accepterSocket = it->second;
            }
        } // 锁在这里释放
        
        if (accepterSocket != INVALID_SOCKET) {
            send(accepterSocket, strMsg.c_str(), strMsg.size(), 0);
        } else {
            std::cout << "[WARN] User " << m.accepter << " not found!" << std::endl;
        }
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
    std::cout << "[SYS] handleClient thread started for socket " << clientSocket << std::endl;
    char buffer[4096];
    //修改接受信息逻辑,实现多次通信
    while (true) {
        std::cout << "[SYS] Waiting for data..." << std::endl;
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        std::cout << "[SYS] recv returned " << bytes << std::endl;
        if (bytes <= 0) {
            std::cout << "[SYS] recv returned " << bytes << ", closing connection" << std::endl;
            break;
        }
        buffer[bytes] = '\0';
        /*recv() 是应用层与传输层的边界操作，取出 TCP 接收窗口内的数据段。数据可能被拆包/粘包，因此应用层需自行定义消息边界（后续协议设计要考虑）。*/
        std::string msg(buffer);
        std::cout << "[SYS] Received raw message: [" << msg << "]" << std::endl;
        Message m = parseMessage(msg);
        std::cout << "[SYS] Parsed - Type:[" << m.type << "] Sender:[" << m.sender << "] Accepter:[" << m.accepter << "] Content:[" << m.content << "]" << std::endl;
        std::cout << "[SYS] Calling handleMessage..." << std::endl;
        handleMessage(m,clientSocket);
        std::cout << "[SYS] handleMessage returned" << std::endl;
        if (m.type == "EXIT") {
            break;  // onExit 已在 handleMessage 中调用，无需重复
        }
    }
    std::cout << "[SYS] Exiting handleClient, closing socket " << clientSocket << std::endl;
    closesocket(clientSocket);//先关闭客户端套接字
    std::cout << "[SYS] handleClient thread ended" << std::endl;
}

int main(){
    //设置控制台支持中文
    SetConsoleOutputCP(65001); // 设置控制台输出为 UTF-8 编码
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
    //接受Client的链接
    std::cout<<"Waiting for client connection..."<<std::endl;
    //接受消息
    while(true){

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