/*******************************************************************************
 * 文件名: sender.cpp
 * 功能概述: RDT协议发送端实现
 * 
 * 整体架构设计:
 * 1. 多线程架构:
 *    - 主线程: 读取文件，发送数据包（流水线发送）
 *    - 监听线程: 接收ACK/SACK，更新拥塞窗口，维护发送缓冲区
 *    - 定时器线程: 检测超时，触发超时重传
 * 
 * 2. 拥塞控制算法（TCP RENO + NewReno增强）:
 *    - 慢启动: cwnd < ssthresh时，每个ACK使cwnd += MSS（指数增长）
 *    - 拥塞避免: cwnd >= ssthresh时，每个RTT使cwnd += MSS（线性增长）
 *    - 快速重传: 收到3个重复ACK，立即重传丢失包
 *    - 快速恢复: 进入后cwnd = ssthresh + 3*MSS，收到重复ACK继续膨胀
 *    - NewReno: 在快速恢复中处理Partial ACK，逐个重传丢失包
 * 
 * 3. 选择重传机制（基于SACK）:
 *    - 接收到SACK块后，标记缓冲区中已确认的包（acked=true）
 *    - 重传时只重传acked=false的包，避免重复重传
 *    - 超时重传也检查acked标志，避免重传已确认的包
 * 
 * 4. 流量控制:
 *    - 接收端通过窗口通告（window_size）限制发送速率
 *    - 发送窗口 = min(SEND_WINDOW_SIZE, cwnd, rwnd)
 * 
 * 5. 连接管理:
 *    - 建立连接: 三次握手（SYN -> SYN-ACK -> ACK）
 *    - 关闭连接: 四次挥手（FIN -> FIN-ACK）
 * 
 * 关键数据结构:
 * - send_buffer: 发送缓冲区，存储未确认的数据包及其状态
 * - base: 最小未确认序列号
 * - next_seq_num: 下一个待发送的序列号
 * - cwnd: 拥塞窗口大小（字节）
 * - rwnd: 接收窗口大小（字节）
 ******************************************************************************/

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../include/rdt_protocol.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <set>
#include <algorithm>
#include <ctime>

using namespace std;

// ==================== 全局状态变量 ====================

// Socket相关
SOCKET sock;                        // UDP套接字
sockaddr_in server_addr;            // 服务器（接收端）地址

// ==================== 拥塞控制和流量控制变量 ====================
// 使用atomic确保多线程安全（主线程、监听线程、定时器线程共享）
atomic<uint32_t> cwnd(MSS);         // 拥塞窗口大小（字节），初始为1个MSS
atomic<uint32_t> ssthresh(65535);   // 慢启动阈值（字节），初始较大
atomic<uint32_t> rwnd(65535);       // 接收窗口大小（字节），从接收端获取
atomic<uint32_t> dupACKcount(0);    // 重复ACK计数器（用于快速重传）
atomic<uint32_t> last_ack(0);       // 最近收到的ACK序列号

// ==================== 序列号管理 ====================
atomic<uint32_t> base(0);           // 最小未确认序列号（滑动窗口左边界）
atomic<uint32_t> next_seq_num(0);   // 下一个待发送的序列号（滑动窗口右边界）
atomic<bool> connection_active(true); // 连接活动标志（用于线程退出）

// ==================== NewReno状态变量 ====================
atomic<uint32_t> recover(0);        // 恢复点：进入快速恢复时记录的最大序列号
atomic<bool> in_fast_recovery(false); // 是否处于快速恢复状态

// ==================== 发送缓冲区 ====================
/**
 * 发送缓冲区条目结构
 * 作用: 存储已发送但未确认的数据包及其元信息
 */
struct SentPacket {
    Packet pkt;                                  // 数据包内容
    chrono::steady_clock::time_point time_sent;  // 发送时间戳（用于超时检测）
    bool acked;                                  // SACK标记：true表示已被SACK确认
};
vector<SentPacket> send_buffer;     // 发送缓冲区（按序列号有序存储）
mutex buffer_mutex;                 // 缓冲区互斥锁（保护多线程访问）

// ==================== 统计和调试 ====================
Statistics stats = {0};             // 性能统计信息
chrono::steady_clock::time_point transmission_start;  // 传输开始时间
bool verbose = false;               // 详细日志开关

// ==================== 核心函数实现 ====================

/**
 * 发送数据包函数
 * 
 * 功能: 计算校验和并通过UDP发送数据包
 * 丢包模拟: 根据LOSS_RATE随机丢弃数据包，但控制包不丢弃
 * 
 * 设计思路:
 * 1. 计算数据包长度：头部 + 数据载荷
 * 2. 计算并填充校验和
 * 3. 判断是否为控制包（SYN/ACK/FIN），控制包不丢弃
 * 4. 对于数据包，根据丢包率决定是否发送
 * 5. 调用sendto发送到UDP套接字
 * 
 * @param pkt 待发送的数据包（引用传递，会修改checksum字段）
 */
void send_packet(Packet& pkt) {
    // 步骤1: 计算数据包总长度
    size_t pkt_size = sizeof(RDTHeader) + pkt.header.len;
    
    // 步骤2: 计算并填充校验和
    pkt.header.checksum = calculate_checksum(&pkt, pkt_size);
    
    // 步骤3: 判断包类型
    bool is_control_packet = (pkt.header.flags & (FLAG_SYN | FLAG_ACK | FLAG_FIN)) != 0;
    bool is_data_packet = pkt.header.len > 0;
    
    // 步骤4: 丢包模拟逻辑
    // 控制包和非数据包始终发送，数据包根据丢包率决定
    if (is_control_packet || !is_data_packet || !should_drop_packet()) {
        // 步骤5: 通过UDP发送
        sendto(sock, (char*)&pkt, static_cast<int>(pkt_size), 0, 
               (sockaddr*)&server_addr, sizeof(server_addr));
    } else {
        // 模拟丢包（可选输出日志）
        // cout << "[Sender] Simulated packet loss at seq=" << pkt.header.seq << endl;
    }
    
    // 记录日志（仅在详细模式下）
    log_packet("SEND", pkt.header, verbose);
}

/**
 * 重传指定序列号的数据包
 * 
 * 功能: 从发送缓冲区中找到指定序列号的包并重传
 * 选择重传: 只重传acked=false的包（避免重传已被SACK确认的包）
 * 
 * 设计思路:
 * 1. 遍历发送缓冲区
 * 2. 找到匹配的序列号且acked=false的包
 * 3. 重新发送该包
 * 4. 更新发送时间戳（重置超时计时器）
 * 5. 增加重传计数
 * 
 * @param seq_num 需要重传的包的序列号
 */
void retransmit_packet(uint32_t seq_num) {
    // 遍历发送缓冲区查找匹配的包
    for (auto& item : send_buffer) {
        if (item.pkt.header.seq == seq_num && !item.acked) {
            // 找到目标包且未被SACK确认
            cout << "[Sender] Retransmitting seq=" << seq_num << endl;
            
            send_packet(item.pkt);  // 重传数据包
            item.time_sent = chrono::steady_clock::now();  // 重置时间戳
            stats.retransmitted_packets++;  // 统计重传次数
            return;  // 找到并重传后立即返回
        }
    }
    // 如果未找到或已被SACK确认，不做任何操作
}

/**
 * 定时器线程函数
 * 
 * 功能: 监控发送缓冲区中最旧的未确认数据包，检测超时并触发重传
 * 
 * 设计思路（TCP超时重传机制）:
 * 1. 周期性检查（每10ms）
 * 2. 只监控缓冲区首部包（最旧的未确认包）
 * 3. 如果超过TIMEOUT_MS且未被SACK确认，触发超时重传
 * 4. 超时后执行TCP RENO超时处理:
 *    - ssthresh = cwnd / 2（记录拥塞点）
 *    - cwnd = 4*MSS（重新进入慢启动，优化为4*MSS）
 *    - 重置重复ACK计数和快速恢复状态
 * 5. 重置所有未确认包的时间戳（防止连锁超时）
 * 
 * 关键优化:
 * - 只监控首包：避免多个包同时超时导致爆发重传
 * - 全局时间戳重置：防止后续包连续超时（domino effect）
 */
void timer_thread_func() {
    while (connection_active) {
        this_thread::sleep_for(chrono::milliseconds(10));  // 每10ms检查一次
        
        lock_guard<mutex> lock(buffer_mutex);  // 加锁保护缓冲区
        if (send_buffer.empty()) continue;     // 缓冲区为空，无需检查

        auto now = chrono::steady_clock::now();
        auto& oldest = send_buffer.front();    // 获取最旧的未确认包
        
        // 检查是否超时：未被SACK确认 且 发送时间超过RTO
        if (!oldest.acked && 
            chrono::duration_cast<chrono::milliseconds>(now - oldest.time_sent).count() > TIMEOUT_MS) {
            
            cout << "[Sender] Timeout! seq=" << oldest.pkt.header.seq 
                 << " cwnd=" << cwnd << endl;
            
            // ========== TCP RENO超时处理 ==========
            // 1. 更新ssthresh = max(cwnd/2, MSS)
            ssthresh = max((uint32_t)MSS, (uint32_t)cwnd / 2);
            
            // 2. 重置cwnd（优化为4*MSS而非1*MSS，减少慢启动时间）
            cwnd = 4 * MSS;
            
            // 3. 重置重复ACK计数器
            dupACKcount = 0;
            
            // 4. 退出NewReno快速恢复状态
            in_fast_recovery = false;
            
            // ========== 重传处理 ==========
            // 重传最旧的未确认包
            send_packet(oldest.pkt);
            stats.retransmitted_packets++;
            
            // ========== 关键优化：防止连锁超时 ==========
            // 超时说明网络状态变化，重置所有未确认包的时间戳
            // 避免后续包在缓冲区中移动到首位后立即超时
            for (auto& item : send_buffer) {
                item.time_sent = now;  // 给所有包一个新的起点
            }
            
            stats.timeouts++;  // 统计超时次数
        }
    }
}

/**
 * 监听线程函数（最复杂的核心逻辑）
 * 
 * 功能: 接收并处理来自接收端的ACK/SACK包，实现拥塞控制和选择重传
 * 
 * 主要职责:
 * 1. 接收ACK/SACK包并验证校验和
 * 2. 解析SACK块，标记已确认的数据包（选择确认）
 * 3. 更新接收窗口（流量控制）
 * 4. 维护发送缓冲区（移除已确认的包）
 * 5. 实现TCP RENO拥塞控制算法
 * 6. 实现NewReno的Partial ACK处理
 * 
 * 拥塞控制状态机:
 * - 正常状态: 慢启动 / 拥塞避免
 * - 快速恢复状态: NewReno快速恢复（处理多个丢包）
 * 
 * 设计思路: 根据ACK类型执行不同的逻辑
 * - 新ACK（ack > base）: 确认新数据，更新窗口
 * - 重复ACK（ack == last_ack）: 检测丢包，触发快速重传
 */
void listener_thread() {
    SACKPacket sack_pkt;  // 接收缓冲区（支持SACK）
    
    while (connection_active) {
        // ========== 步骤1: 接收ACK包 ==========
        int bytes = recvfrom(sock, (char*)&sack_pkt, sizeof(SACKPacket), 0, NULL, NULL);
        if (bytes == SOCKET_ERROR) continue;  // 接收失败，继续等待

        // ========== 步骤2: 验证校验和 ==========
        size_t expected_size = sizeof(RDTHeader) + sack_pkt.header.len;
        if (calculate_checksum(&sack_pkt, expected_size) != sack_pkt.header.checksum) {
            continue;  // 校验和错误，丢弃该ACK
        }

        // ========== 步骤3: 提取关键信息 ==========
        uint32_t ack = sack_pkt.header.ack;  // 累积确认号
        rwnd = (uint32_t)sack_pkt.header.window_size * MSS;  // 更新接收窗口（流量控制）

        log_packet("RECV-ACK", sack_pkt.header, verbose);  // 记录日志

        lock_guard<mutex> lock(buffer_mutex);  // 加锁保护缓冲区
        
        // ========== 步骤4: 处理SACK块（选择确认） ==========
        // 功能: 标记缓冲区中已被接收端确认的数据包
        // 作用: 实现选择重传，避免重传已收到的乱序数据
        if (sack_pkt.header.flags & FLAG_SACK) {
            for (int i = 0; i < sack_pkt.header.sack_count; i++) {
                uint32_t sack_start = sack_pkt.sack_blocks[i].start;  // SACK块起始序号
                uint32_t sack_end = sack_pkt.sack_blocks[i].end;      // SACK块结束序号
                
                // 遍历发送缓冲区，标记SACK范围内的包为已确认
                for (auto& item : send_buffer) {
                    if (item.pkt.header.seq >= sack_start && 
                        item.pkt.header.seq < sack_end) {
                        item.acked = true;  // 标记为已确认（不再重传）
                    }
                }
            }
        }
        
        // ========== 步骤5: 处理ACK（根据ACK类型分类处理） ==========
        
        if (ack > base) {
            // ===== 情况1: 新ACK（确认了新数据） =====
            if (verbose) {
                cout << "[Sender] New ACK=" << ack << " cwnd=" << cwnd;
            }
            
            // ----- 子步骤5.1: 移除已确认的数据包 -----
            // 功能: 从缓冲区头部移除所有已被累积确认的包
            // 累积确认: ack表示期望接收的下一个字节，所以 < ack的都已确认
            {
                while (!send_buffer.empty()) {
                    uint32_t pkt_end = send_buffer.front().pkt.header.seq + 
                                       send_buffer.front().pkt.header.len;
                    if (pkt_end <= ack) {
                        send_buffer.erase(send_buffer.begin());  // 移除已确认的包
                    } else {
                        break;  // 遇到未确认的包，停止移除
                    }
                }
                
                // ----- RFC 6298关键修复: 重启重传定时器 -----
                // 收到新ACK时，给新的最旧包一个完整的超时窗口
                // 避免旧包的计时器影响新包的超时判断
                if (!send_buffer.empty()) {
                    send_buffer.front().time_sent = chrono::steady_clock::now();
                }
            }

            // ----- 子步骤5.2: NewReno拥塞控制逻辑 -----
            if (in_fast_recovery) {
                // ===== 快速恢复状态中收到ACK =====
                
                if (ack >= recover) {
                    // ***** Full ACK: 完全确认 *****
                    // 含义: ACK覆盖了进入快速恢复前发送的所有数据
                    // 操作: 退出快速恢复，恢复正常拥塞控制
                    if (verbose) cout << " -> (Full ACK, Exit Fast Recovery)" << endl;
                    
                    in_fast_recovery = false;  // 退出快速恢复
                    dupACKcount = 0;           // 重置重复ACK计数
                    
                    // 设置cwnd = ssthresh（从拥塞避免阶段继续）
                    cwnd = ssthresh.load(); 
                    
                } else {
                    // ***** Partial ACK: 部分确认 *****
                    // 含义: ACK确认了一些新数据，但未到达recover点
                    // 原因: 窗口中有多个丢包，当前ACK只确认到第一个丢包
                    // 操作: 继续快速恢复，立即重传下一个丢失的包
                    if (verbose) cout << " -> (Partial ACK, Stay in Fast Recovery)" << endl;
                    
                    // NewReno关键特性: 立即重传下一个丢失段
                    // ack指向的就是下一个丢失的包的序列号
                    cout << "[Sender] Partial ACK received. Retransmitting seq=" << ack << endl;
                    retransmit_packet(ack);
                    
                    // 继续保持快速恢复状态，不退出
                    // dupACKcount不重置（保持快速恢复的窗口膨胀）
                }
                
            } else {
                // ===== 正常状态（非快速恢复）中收到新ACK =====
                
                base = ack;          // 更新滑动窗口左边界
                last_ack = ack;      // 记录最新ACK
                dupACKcount = 0;     // 重置重复ACK计数

                // ----- TCP RENO拥塞控制算法 -----
                if (cwnd < ssthresh) {
                    // ***** 慢启动阶段 *****
                    // 条件: cwnd < ssthresh
                    // 增长: 每收到一个ACK，cwnd增加MSS
                    // 结果: 指数增长（每RTT翻倍）
                    cwnd += MSS;
                    if (verbose) cout << " -> " << cwnd << " (Slow Start)" << endl;
                    
                } else {
                    // ***** 拥塞避免阶段 *****
                    // 条件: cwnd >= ssthresh
                    // 增长: 每RTT增加一个MSS（线性增长）
                    // 实现: 每个ACK增加 MSS*MSS/cwnd
                    uint32_t increment = max((uint32_t)1, MSS * MSS / cwnd);
                    cwnd += increment;
                    if (verbose) cout << " -> " << cwnd << " (Congestion Avoidance)" << endl;
                }
            }
            
            base = ack;      // 确保base更新（兼容性处理）
            last_ack = ack;  // 更新last_ack

        } else if (ack == last_ack) {
            // ===== 情况2: 重复ACK（检测到可能的丢包） =====
            // 含义: 收到的ACK序号与上一个ACK相同
            // 原因: 接收端收到乱序数据，重复发送累积ACK
            // 作用: 通过重复ACK的数量判断是否发生丢包
            
            dupACKcount++;           // 重复ACK计数器加1
            stats.duplicate_acks++;  // 统计重复ACK数量
            
            if (dupACKcount == 2) {
                // ***** 收到第3个重复ACK（前2个 + 当前1个）*****
                // 判断: 极可能发生丢包
                // 操作: 触发快速重传和快速恢复
                
                cout << "[Sender] 2 Dup ACKs! Fast Retransmit seq=" << ack 
                     << " cwnd=" << cwnd << " -> ";
                
                // ----- 进入快速恢复状态（NewReno）-----
                recover = next_seq_num.load();  // 记录恢复点（当前最大序号）
                in_fast_recovery = true;         // 设置快速恢复标志
                
                // ----- TCP RENO快速重传算法 -----
                // 1. 更新ssthresh = max(cwnd/2, MSS)
                ssthresh = max((uint32_t)MSS, (uint32_t)cwnd / 2);
                
                // 2. 设置cwnd = ssthresh + 3*MSS
                //    原因: +3*MSS是因为已有3个重复ACK表示3个包已离开网络
                cwnd = ssthresh.load() + 3 * MSS;
                
                cout << cwnd << endl;
                
                // 3. 立即重传第一个丢失的包（ack指向的包）
                retransmit_packet(ack);
                
            } else if (dupACKcount > 2) {
                // ***** 收到更多的重复ACK（第4个、第5个...）*****
                // 状态: 已在快速恢复中
                // 操作: 窗口膨胀（Inflate Window）
                // 原因: 每个重复ACK表示一个包已离开网络，可以发送新包
                cwnd += MSS;
            }
        }
        
        // ========== 步骤6: 处理FIN包（连接关闭） ==========
        if (sack_pkt.header.flags & FLAG_FIN) {
            cout << "[Sender] FIN-ACK received. Connection closed." << endl;
            connection_active = false;  // 通知所有线程退出
            break;  // 退出监听循环
        }
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    string filename = "test_file.dat";
    string local_ip = "0.0.0.0";  // Bind to any interface by default
    int local_port = 0;            // Let system assign port
    string target_ip = DEFAULT_IP;
    int target_port = DEFAULT_PORT;
    
    // Parse arguments with validation
    if (argc > 1) {
        int win = atoi(argv[1]);
        if (win > 0 && win <= 256) {
            SEND_WINDOW_SIZE = win;
        } else if (win == 0 && argv[1][0] != '0') {
            // User might have provided filename as first arg
            cerr << "错误: 第一个参数应该是窗口大小(数字)，不是文件名!" << endl;
            cerr << "正确格式: sender.exe [窗口] [丢包率] [文件] [本地IP] [本地端口] [目标IP] [目标端口]" << endl;
            cerr << "示例: sender.exe 32 0.0 test.jpg 127.0.0.1 12000 127.0.0.1 12001" << endl;
            cerr << "或直接运行脚本: scripts\\test_router_localhost.bat" << endl;
            return 1;
        }
    }
    if (argc > 2) LOSS_RATE = atof(argv[2]);
    if (argc > 3) filename = argv[3];
    if (argc > 4) local_ip = argv[4];
    if (argc > 5) local_port = atoi(argv[5]);
    if (argc > 6) target_ip = argv[6];
    if (argc > 7) target_port = atoi(argv[7]);
    if (argc > 8) verbose = (strcmp(argv[8], "-v") == 0);
    
    // Validate window size
    if (SEND_WINDOW_SIZE <= 0) {
        cerr << "错误: 发送窗口大小必须 > 0! 当前值: " << SEND_WINDOW_SIZE << endl;
        cerr << "请使用正确的参数格式" << endl;
        return 1;
    }

    // Allow multiple packets in flight from the start (avoids single-packet pipe)
    cwnd = min<uint32_t>(static_cast<uint32_t>(SEND_WINDOW_SIZE) * MSS, static_cast<uint32_t>(MSS * 10));
    
    cout << "========== RDT Sender ==========" << endl;
    cout << "Send Window Size: " << SEND_WINDOW_SIZE << " packets" << endl;
    cout << "Packet Loss Rate: " << (LOSS_RATE * 100) << "%" << endl;
    cout << "Input File: " << filename << endl;
    cout << "Local: " << local_ip << ":" << local_port << endl;
    cout << "Target: " << target_ip << ":" << target_port << endl;
    cout << "================================" << endl;

    // Initialize Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    // Bind to local address if specified
    if (local_port > 0) {
        sockaddr_in local_addr;
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = (local_ip == "0.0.0.0") ? INADDR_ANY : inet_addr(local_ip.c_str());
        local_addr.sin_port = htons(local_port);
        if (bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
            cerr << "[Sender] Bind failed!" << endl;
            closesocket(sock);
            WSACleanup();
            return 1;
        }
        cout << "[Sender] Bound to " << local_ip << ":" << local_port << endl;
    }
    
    // Set target (Router or direct Server)
    // 设置目标地址（Router或直接连接到Server）
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(target_ip.c_str());
    server_addr.sin_port = htons(target_port);

    // ==================== 三次握手建立连接 ====================
    /**
     * TCP三次握手流程：
     * 
     * 发送端(Client)            接收端(Server)
     *      |                            |
     *      |-------- SYN (seq=0) ------>|  步骤1: 发送端发送SYN请求建立连接
     *      |                            |
     *      |<-- SYN-ACK (seq=0,ack=1) --|  步骤2: 接收端确认并发送自己的SYN
     *      |                            |
     *      |-------- ACK (ack=1) ------>|  步骤3: 发送端确认，连接建立
     *      |                            |
     *      |   开始数据传输   |
     * 
     * 为什么需要三次握手？
     * 1. 确认双方都有发送和接收能力
     * 2. 同步双方的初始序列号
     * 3. 防止旧的连接请求导致错误
     */
    cout << "[Sender] Starting handshake..." << endl;
    
    // ----- 步骤1: 构造并发送SYN包 -----
    Packet syn_pkt;
    memset(&syn_pkt, 0, sizeof(syn_pkt));  // 清零所有字段
    syn_pkt.header.seq = 0;                 // 初始序列号为0
    syn_pkt.header.flags = FLAG_SYN;        // 设置SYN标志
    send_packet(syn_pkt);                   // 发送SYN包
    
    // ----- 步骤2: 等待接收SYN-ACK（带超时重传） -----
    Packet syn_ack;  // 用于接收SYN-ACK包
    int len = sizeof(server_addr);
    
    // 使用select实现超时机制
    auto handshake_start = chrono::steady_clock::now();
    while (true) {
        // 设置文件描述符集（用于select）
        fd_set read_fds;
        FD_ZERO(&read_fds);          // 清空集合
        FD_SET(sock, &read_fds);     // 将socket添加到集合
        
        // 设置超时时间
        struct timeval timeout;
        timeout.tv_sec = 0;                    // 0秒
        timeout.tv_usec = TIMEOUT_MS * 1000;   // 300毫秒 = 300,000微秒
        
        // select: 等待socket可读或超时
        // 返回值: >0表示有数据，0表示超时，-1表示错误
        int activity = select(0, &read_fds, NULL, NULL, &timeout);
        
        if (activity > 0) {
            // 有数据到达，尝试接收SYN-ACK
            int res = recvfrom(sock, (char*)&syn_ack, sizeof(Packet), 0, 
                              (sockaddr*)&server_addr, &len);
            
            // 验证SYN-ACK包的合法性
            if (res > 0 &&                                      // 接收成功
                (syn_ack.header.flags & (FLAG_SYN | FLAG_ACK)) && // 有SYN和ACK标志
                syn_ack.header.ack == 1) {                     // ACK确认我们的seq+1
                
                cout << "[Sender] Handshake complete." << endl;
                
                // 初始化连接参数
                base = 1;                                        // 最小未确认序列号
                next_seq_num = 1;                                // 下一个发送序列号
                rwnd = syn_ack.header.window_size * MSS;         // 获取接收窗口大小
                break;  // 退出循环，握手成功
            }
        } else {
            // select超时，未收到SYN-ACK
            cout << "[Sender] Handshake timeout. Retransmitting SYN..." << endl;
            send_packet(syn_pkt);  // 重传SYN包
            // 继续循环等待
        }
    }
    
    // ----- 步骤3: 发送最后ACK包，完成三次握手 -----
    Packet ack_pkt;
    memset(&ack_pkt, 0, sizeof(ack_pkt));
    ack_pkt.header.seq = 1;                      // 我们的序列号
    ack_pkt.header.ack = syn_ack.header.seq + 1; // 确认对方的SYN
    ack_pkt.header.flags = FLAG_ACK;             // 只有ACK标志
    send_packet(ack_pkt);
    
    // 至此三次握手完成，连接建立！

    // Start threads
    thread listener(listener_thread);
    listener.detach();
    
    thread timer(timer_thread_func);
    timer.detach();

    // Prepare input file
    ifstream infile(filename, ios::binary);
    if (!infile.is_open()) {
        cout << "[Sender] Creating test file..." << endl;
        ofstream temp(filename, ios::binary);
        for (int i = 0; i < 1024; i++) {
            char line[1024];
            snprintf(line, sizeof(line), "Line %04d: This is test data for RDT protocol performance evaluation.\n", i);
            temp.write(line, strlen(line));
        }
        temp.close();
        infile.open(filename, ios::binary);
    }

    // Get file size
    infile.seekg(0, ios::end);
    size_t file_size = infile.tellg();
    infile.seekg(0, ios::beg);
    cout << "[Sender] File size: " << file_size << " bytes" << endl;

    // ==================== 流水线发送数据 ====================
    /**
     * 流水线传输机制（Pipelined Transmission）
     * 
     * 核心思想：
     * 不等待前一个包的ACK返回，连续发送多个数据包，充分利用网络带宽
     * 
     * 与停等协议对比：
     * - 停等协议（Stop-and-Wait）：发送1个包 → 等待ACK → 再发送下一个包
     *   缺点：网络利用率低，吞吐量 = MSS / RTT
     * 
     * - 流水线协议（Pipeline）：连续发送N个包 → 后台接收ACK → 滑动窗口前进
     *   优点：网络利用率高，吞吐量 = Window_Size * MSS / RTT
     * 
     * 窗口机制：
     * [base ... next_seq_num) 表示已发送但未确认的数据（在途数据）
     * 窗口大小 = min(发送窗口, 拥塞窗口, 接收窗口)
     * 
     * 流程图：
     * 
     *   文件读取 → 窗口检查 → 发送数据包 → 加入缓冲区 → 序列号前进
     *      ↑                                                    ↓
     *      └────────────────← 继续发送下一个包 ←────────────────┘
     *                    （不等待ACK，实现流水线）
     * 
     * 同时：监听线程在后台接收ACK，移除已确认的包，释放窗口空间
     */
    transmission_start = chrono::steady_clock::now();
    char buffer[MSS];  // 读取缓冲区，每次读取MSS字节
    
    // ========== 主循环：流水线发送所有数据 ==========
    while (infile.read(buffer, MSS) || infile.gcount() > 0) {
        int bytes_read = static_cast<int>(infile.gcount());  // 实际读取的字节数
        
        // ========== 步骤1: 流量控制和拥塞控制 ==========
        // 功能：限制在途数据量，防止发送速度过快导致：
        //      1. 接收端缓冲区溢出（流量控制）
        //      2. 网络拥塞（拥塞控制）
        while (true) {
            // 计算当前在途数据量（已发送但未确认的字节数）
            // inflight = 下一个待发送序列号 - 最小未确认序列号
            uint32_t inflight = next_seq_num - base;
            
            // 计算有效发送窗口（三个窗口取最小值）
            // win = min(固定窗口, 拥塞窗口, 接收窗口)
            // - SEND_WINDOW_SIZE: 配置的固定窗口大小（包数）
            // - cwnd: 拥塞窗口（字节），由TCP RENO算法动态调整
            // - rwnd: 接收窗口（字节），由接收端通告
            uint32_t win = min((uint32_t)SEND_WINDOW_SIZE * MSS, 
                              min((uint32_t)cwnd, (uint32_t)rwnd));
            
            // 如果窗口未满，可以发送新包
            if (inflight < win) break;
            
            // 窗口已满，主动让出CPU，等待监听线程收到ACK释放窗口
            // 使用yield而非sleep以保持高吞吐量（减少等待延迟）
            this_thread::yield();
        }

        // ========== 步骤2: 构造数据包 ==========
        Packet data_pkt;
        memset(&data_pkt, 0, sizeof(data_pkt));       // 清零所有字段
        data_pkt.header.seq = next_seq_num;           // 设置序列号
        data_pkt.header.len = bytes_read;             // 设置数据长度
        memcpy(data_pkt.data, buffer, bytes_read);    // 复制数据到包中

        // ========== 步骤3: 发送数据包并加入缓冲区 ==========
        // 使用互斥锁保护缓冲区（多线程访问：主线程添加，监听线程移除）
        {
            lock_guard<mutex> lock(buffer_mutex);
            
            // 立即发送数据包（通过UDP，不等待ACK）
            // ⭐ 这里就是流水线的关键：发送后不阻塞，继续循环发送下一个包
            send_packet(data_pkt);
            
            // 将数据包加入发送缓冲区，用于：
            // 1. 超时重传：定时器线程检测超时
            // 2. 快速重传：监听线程收到3个重复ACK时重传
            // 3. 选择重传：SACK标记已确认的包
            // 结构：{数据包, 发送时间戳, SACK确认标志}
            send_buffer.push_back({data_pkt, chrono::steady_clock::now(), false});
        }
        
        // ========== 可选：流量整形（Pacing）==========
        // 注释掉的代码用于限制发送速率，避免突发流量
        // 在高速网络中为了最大化吞吐量，这里移除了延迟
        // this_thread::sleep_for(chrono::microseconds(500));
        
        // ========== 步骤4: 更新统计和序列号 ==========
        stats.total_packets++;              // 统计发送的包数
        stats.total_bytes += bytes_read;    // 统计发送的字节数
        next_seq_num += bytes_read;         // 序列号前进（准备发送下一个包）
        
        // ⭐ 循环继续，立即发送下一个包（流水线的核心）
        // 此时前面发送的包可能还未收到ACK，但我们已经在发送新包了
    }

    // ==================== 等待所有数据被确认 ====================
    /**
     * 为什么需要等待所有ACK？
     * 
     * 1. 确保数据完整性：
     *    主循环结束时，文件已全部发送，但可能还有数据包在网络中传输
     *    或者某些包丢失需要重传，必须等待所有包都被接收端确认
     * 
     * 2. 判断条件：base < next_seq_num
     *    - base: 最小未确认序列号（由监听线程更新）
     *    - next_seq_num: 下一个待发送序列号（文件末尾）
     *    - 当 base == next_seq_num 时，说明所有数据都已确认
     * 
     * 3. 等待机制：
     *    轮询检查 base，每50ms检查一次
     *    期间监听线程持续接收ACK，逐步推进base
     */
    cout << "[Sender] Waiting for all ACKs..." << endl;
    while (base < next_seq_num) {
        this_thread::sleep_for(chrono::milliseconds(50));  // 轮询等待
        // 监听线程收到ACK后会更新base，直到base追上next_seq_num
    }

    // ========== 计算传输时间 ==========
    auto transmission_end = chrono::steady_clock::now();
    stats.transmission_time_ms = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(
        transmission_end - transmission_start).count());

    // ==================== 四次挥手关闭连接 ====================
    /**
     * 标准TCP四次挥手流程：
     * 
     * 发送端(主动关闭)           接收端(被动关闭)
     *      |                            |
     *      |------ FIN (seq=X) -------->|  步骤1: 主动方发送FIN
     *      |                            |
     *      |<----- ACK (ack=X+1) -------|  步骤2: 被动方确认FIN
     *      |                            |
     *      |<----- FIN (seq=Y) ---------|  步骤3: 被动方发送FIN
     *      |                            |
     *      |------ ACK (ack=Y+1) ------>|  步骤4: 主动方确认FIN
     *      |                            |
     *      |   连接完全关闭   |
     * 
     * 为什么需要四次挥手？
     * - 连接是全双工的，每个方向都需要单独关闭
     * - 被动方可能还有数据要发送，需要时间处理完
     * - 确保双方都知道连接已完全关闭
     */
    cout << "[Sender] Starting 4-way handshake to close connection..." << endl;
    
    // ----- 步骤1: 发送FIN包（主动关闭） -----
    cout << "[Sender] Step 1: Sending FIN..." << endl;
    Packet fin_pkt;
    memset(&fin_pkt, 0, sizeof(fin_pkt));
    fin_pkt.header.seq = next_seq_num;
    fin_pkt.header.flags = FLAG_FIN;
    send_packet(fin_pkt);

    // ----- 步骤2: 等待接收端的ACK -----
    cout << "[Sender] Step 2: Waiting for ACK of FIN..." << endl;
    Packet ack_of_fin;
    bool received_ack = false;
    auto fin_start = chrono::steady_clock::now();
    
    while (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - fin_start).count() < 3) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000; // 500ms
        
        int activity = select(0, &read_fds, NULL, NULL, &timeout);
        
        if (activity > 0) {
            int res = recvfrom(sock, (char*)&ack_of_fin, sizeof(Packet), 0, NULL, NULL);
            if (res > 0 && (ack_of_fin.header.flags & FLAG_ACK) && 
                ack_of_fin.header.ack == next_seq_num + 1) {
                cout << "[Sender] Step 2: Received ACK of FIN" << endl;
                received_ack = true;
                break;
            }
        }
    }
    
    if (!received_ack) {
        cout << "[Sender] Warning: Did not receive ACK of FIN, continuing anyway..." << endl;
    }

    // ----- 步骤3: 等待接收端的FIN -----
    cout << "[Sender] Step 3: Waiting for receiver's FIN..." << endl;
    Packet recv_fin;
    bool received_fin = false;
    fin_start = chrono::steady_clock::now();
    
    while (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - fin_start).count() < 3) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000; // 500ms
        
        int activity = select(0, &read_fds, NULL, NULL, &timeout);
        
        if (activity > 0) {
            int res = recvfrom(sock, (char*)&recv_fin, sizeof(Packet), 0, NULL, NULL);
            if (res > 0 && (recv_fin.header.flags & FLAG_FIN)) {
                cout << "[Sender] Step 3: Received FIN from receiver" << endl;
                received_fin = true;
                
                // ----- 步骤4: 发送ACK确认接收端的FIN -----
                cout << "[Sender] Step 4: Sending ACK of receiver's FIN..." << endl;
                Packet final_ack;
                memset(&final_ack, 0, sizeof(final_ack));
                final_ack.header.seq = next_seq_num + 1;
                final_ack.header.ack = recv_fin.header.seq + 1;
                final_ack.header.flags = FLAG_ACK;
                send_packet(final_ack);
                
                cout << "[Sender] 4-way handshake complete. Connection closed." << endl;
                break;
            }
        }
    }
    
    if (!received_fin) {
        cout << "[Sender] Warning: Did not receive FIN from receiver" << endl;
    }

    // 等待一小段时间确保最后的ACK被接收
    this_thread::sleep_for(chrono::milliseconds(200));
    connection_active = false;

    // Print statistics
    stats.print();

    closesocket(sock);
    WSACleanup();
    return 0;
}
