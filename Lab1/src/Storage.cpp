#include "../include/Storage.h"
#include "sqlite3.h"
#include <iostream>
#include <ctime>
#include <algorithm>

// ========== 构造函数和析构函数 ==========

Storage::Storage(const std::string& userName) {
    // 每个用户独立的数据库文件（存储在 data 目录）
    dbPath = "data\\" + userName + "_chat.db";
    db = nullptr;
}

Storage::~Storage() {
    close();
}

// ========== 初始化数据库 ==========

bool Storage::init() {
    // 确保 data 目录存在
    system("if not exist data mkdir data");
    
    // 打开数据库（如果不存在会自动创建）
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[Storage] 无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    std::cout << "[Storage] 数据库已打开: " << dbPath << std::endl;
    
    // 创建消息表
    const char* createMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT NOT NULL,
            session_type TEXT NOT NULL,
            sender TEXT NOT NULL,
            receiver TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            message_type TEXT NOT NULL,
            is_read INTEGER DEFAULT 0
        );
    )";
    
    if (!executeSQL(createMessagesTable)) {
        std::cerr << "[Storage] 创建消息表失败" << std::endl;
        return false;
    }
    
    // 创建会话表（用于保存会话元数据）
    const char* createSessionsTable = R"(
        CREATE TABLE IF NOT EXISTS sessions (
            session_id TEXT PRIMARY KEY,
            session_type TEXT NOT NULL,
            last_sync_time INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL
        );
    )";
    
    if (!executeSQL(createSessionsTable)) {
        std::cerr << "[Storage] 创建会话表失败" << std::endl;
        return false;
    }
    
    // 创建索引（加速查询）
    executeSQL("CREATE INDEX IF NOT EXISTS idx_session_time ON messages(session_id, timestamp);");
    executeSQL("CREATE INDEX IF NOT EXISTS idx_timestamp ON messages(timestamp);");
    
    std::cout << "[Storage] 数据库表初始化完成" << std::endl;
    return true;
}

void Storage::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        std::cout << "[Storage] 数据库已关闭" << std::endl;
    }
}

// ========== 辅助函数 ==========

bool Storage::executeSQL(const char* sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "[Storage] SQL 错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ========== 消息存储相关函数 ==========

bool Storage::saveMessage(const Message& msg, const std::string& sessionId, SessionType sessionType) {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    const char* sql = R"(
        INSERT INTO messages 
        (session_id, session_type, sender, receiver, content, timestamp, message_type) 
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 准备插入语句失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // 绑定参数
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, sessionType == ST_GROUP ? "GROUP" : "PRIVATE", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, msg.sender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, msg.accepter.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, (int64_t)time(nullptr));  // 当前时间戳
    sqlite3_bind_text(stmt, 7, msg.type.c_str(), -1, SQLITE_TRANSIENT);
    
    // 执行
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    
    if (!success) {
        std::cerr << "[Storage] 插入消息失败: " << sqlite3_errmsg(db) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Message> Storage::loadHistory(const std::string& sessionId, int limit) {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<Message> history;
    
    const char* sql = R"(
        SELECT sender, receiver, content, message_type, timestamp
        FROM messages 
        WHERE session_id = ? 
        ORDER BY timestamp DESC 
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 查询历史失败: " << sqlite3_errmsg(db) << std::endl;
        return history;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg.sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        msg.accepter = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        msg.timestamp = sqlite3_column_int64(stmt, 4);  // 🔥 读取时间戳
        
        history.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    
    // 反转顺序（因为查询是 DESC，最新的在前）
    std::reverse(history.begin(), history.end());
    
    return history;
}

std::vector<Message> Storage::getNewMessages(const std::string& sessionId, int64_t afterTimestamp) {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<Message> newMessages;
    
    const char* sql = R"(
        SELECT sender, receiver, content, message_type, timestamp
        FROM messages 
        WHERE session_id = ? AND timestamp > ? 
        ORDER BY timestamp ASC
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 查询新消息失败: " << sqlite3_errmsg(db) << std::endl;
        return newMessages;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, afterTimestamp);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg.sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        msg.accepter = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        msg.timestamp = sqlite3_column_int64(stmt, 4);  // 🔥 读取时间戳
        
        newMessages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    return newMessages;
}

int64_t Storage::getLastMessageTime(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    const char* sql = "SELECT MAX(timestamp) FROM messages WHERE session_id = ?";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 查询最后消息时间失败: " << sqlite3_errmsg(db) << std::endl;
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    
    int64_t lastTime = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        lastTime = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return lastTime;
}

// ========== 会话管理函数 ==========

bool Storage::saveSession(const std::string& sessionId, SessionType type) {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    const char* sql = R"(
        INSERT OR IGNORE INTO sessions (session_id, session_type, created_at) 
        VALUES (?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 保存会话失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, type == ST_GROUP ? "GROUP" : "PRIVATE", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (int64_t)time(nullptr));
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

std::vector<std::string> Storage::loadSessions() {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<std::string> sessions;
    
    const char* sql = R"(
        SELECT DISTINCT session_id 
        FROM messages 
        GROUP BY session_id 
        ORDER BY MAX(timestamp) DESC
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 加载会话列表失败: " << sqlite3_errmsg(db) << std::endl;
        return sessions;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* sessionId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (sessionId) {
            sessions.push_back(sessionId);
        }
    }
    
    sqlite3_finalize(stmt);
    return sessions;
}

SessionType Storage::getSessionType(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    const char* sql = "SELECT session_type FROM sessions WHERE session_id = ?";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        // 如果查询失败，尝试从会话名推断
        return (sessionId == "ALL") ? ST_GROUP : ST_PRIVATE;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    
    SessionType type = ST_GROUP;  // 默认
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* typeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (typeStr) {
            type = (strcmp(typeStr, "GROUP") == 0) ? ST_GROUP : ST_PRIVATE;
        }
    } else {
        // 数据库中没有记录，根据名称推断
        type = (sessionId == "ALL") ? ST_GROUP : ST_PRIVATE;
    }
    
    sqlite3_finalize(stmt);
    return type;
}

bool Storage::updateLastSyncTime(const std::string& sessionId, int64_t timestamp) {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    const char* sql = R"(
        UPDATE sessions 
        SET last_sync_time = ? 
        WHERE session_id = ?
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 更新同步时间失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

// ========== 统计功能 ==========

int Storage::getUnreadCount(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    const char* sql = "SELECT COUNT(*) FROM messages WHERE session_id = ? AND is_read = 0";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Storage] 查询未读数量失败: " << sqlite3_errmsg(db) << std::endl;
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

bool Storage::clearAllData() {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    bool success = true;
    success &= executeSQL("DELETE FROM messages");
    success &= executeSQL("DELETE FROM sessions");
    
    if (success) {
        std::cout << "[Storage] 所有数据已清除" << std::endl;
    }
    
    return success;
}








