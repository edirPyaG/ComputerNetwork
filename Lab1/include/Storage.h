#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <vector>
#include <mutex>
#include "Common.h"

// 前向声明 SQLite 类型（避免在头文件中包含 sqlite3.h）
struct sqlite3;

class Storage {
private:
    sqlite3* db;                // 数据库连接对象
    std::string dbPath;         // 数据库文件路径
    std::mutex dbMutex;         // 线程安全锁
    
    // 内部辅助函数
    bool executeSQL(const char* sql);

public:
    // 构造函数和析构函数
    Storage(const std::string& userName);
    ~Storage();
    
    // 初始化数据库（创建表和索引）
    bool init();
    
    // 关闭数据库
    void close();
    
    // ========== 消息存储相关函数 ==========
    
    // 保存消息到数据库
    bool saveMessage(const Message& msg, const std::string& sessionId, SessionType sessionType);
    
    // 加载会话历史（最近 N 条消息）
    std::vector<Message> loadHistory(const std::string& sessionId, int limit = 100);
    
    // 获取某个会话在指定时间戳之后的新消息（用于同步）
    std::vector<Message> getNewMessages(const std::string& sessionId, int64_t afterTimestamp);
    
    // 获取某个会话的最后消息时间戳
    int64_t getLastMessageTime(const std::string& sessionId);
    
    // ========== 会话管理函数 ==========
    
    // 保存会话元数据
    bool saveSession(const std::string& sessionId, SessionType type);
    
    // 加载所有会话列表（按最后消息时间排序）
    std::vector<std::string> loadSessions();
    
    // 获取会话类型
    SessionType getSessionType(const std::string& sessionId);
    
    // 更新会话的最后同步时间
    bool updateLastSyncTime(const std::string& sessionId, int64_t timestamp);
    
    // ========== 统计功能 ==========
    
    // 获取未读消息数量
    int getUnreadCount(const std::string& sessionId);
    
    // 清除所有数据（用于测试）
    bool clearAllData();
};

#endif // STORAGE_H



