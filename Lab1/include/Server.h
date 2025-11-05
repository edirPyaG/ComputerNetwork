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

