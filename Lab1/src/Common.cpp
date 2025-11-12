#include "../include/Common.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include<vector>

//定义解封装函数
Message parseMessage(const std::string &strMsg){
        Message m;
        size_t start=0;
        size_t pos;
        //拆分解析消息
        std::vector<std::string> parts;
        while((pos=strMsg.find('|',start))!=std::string::npos){
            parts.push_back(strMsg.substr(start,pos-start));
            start=pos+1;
        }
        // 添加最后一个字段（从最后一个 | 之后到字符串末尾）
        parts.push_back(strMsg.substr(start));

        //判断后填结构体
        if(parts.size()>0) m.type=parts[0];
        if(parts.size()>1) m.sender=parts[1];
        if(parts.size()>2) m.accepter=parts[2];       
        if(parts.size()>3) m.content=parts[3];

        return m;
}
//定义封装函数
std::string buildMessage(const Message& m) {
    return m.type + "|" + m.sender + "|" + m.accepter + "|" + m.content;
}

