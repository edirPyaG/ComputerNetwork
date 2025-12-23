/*******************************************************************************
 * 文件名: rdt_protocol.h
 * 功能概述: RDT可靠数据传输协议头文件
 * 
 * 整体设计思路:
 * 1. 基于UDP实现面向连接的可靠传输协议（类似TCP）
 * 2. 实现三次握手建立连接，四次挥手关闭连接
 * 3. 使用16位Internet校验和进行差错检测
 * 4. 支持选择确认（SACK）机制，提高重传效率
 * 5. 实现TCP RENO拥塞控制算法（慢启动、拥塞避免、快速重传、快速恢复）
 * 6. 支持流量控制，通过接收窗口通告机制
 * 7. 提供丢包模拟功能，用于性能测试
 * 
 * 核心数据结构:
 * - RDTHeader: 协议头部（12字节），包含序号、确认号、标志位等
 * - SACKBlock: 选择确认块，标识已接收的乱序数据范围
 * - Packet: 完整数据包 = 头部 + 数据载荷（最大1024字节）
 * - SACKPacket: 带SACK信息的ACK包
 ******************************************************************************/

#ifndef RDT_PROTOCOL_H
#define RDT_PROTOCOL_H

#include <cstdint>
#include <cstring>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <random>

// Link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

// ==================== 协议常量定义 ====================
#define MSS 1024                    // 最大分段大小（Maximum Segment Size）
#define DEFAULT_PORT 8888           // 默认通信端口
#define DEFAULT_IP "127.0.0.1"      // 默认IP地址（本地回环）
#define TIMEOUT_MS 500              // 重传超时时间（毫秒）
#define MAX_SACK_BLOCKS 8           // 最大SACK块数量（避免ACK包过大）

// ==================== 控制标志位定义 ====================
// 用于标识数据包的类型和状态
#define FLAG_SYN 0x01               // 同步标志：建立连接请求
#define FLAG_ACK 0x02               // 确认标志：确认接收
#define FLAG_FIN 0x04               // 结束标志：关闭连接
#define FLAG_SACK 0x08              // 选择确认标志：携带SACK信息

// ==================== 全局配置变量 ====================
// 这些变量在config.cpp中定义，可通过命令行参数配置
extern int SEND_WINDOW_SIZE;        // 发送窗口大小（单位：数据包）
extern int RECV_WINDOW_SIZE;        // 接收窗口大小（单位：数据包）
extern double LOSS_RATE;            // 丢包率模拟（0.0-1.0，如0.05表示5%丢包）

// ==================== 协议头部结构定义 ====================
// 使用1字节对齐，确保结构体紧凑，避免内存填充
#pragma pack(push, 1)

/**
 * RDT协议头部结构（12字节）
 * 设计思路：参考TCP头部，简化实现，保留核心字段
 * 字段说明：
 * - seq: 序列号（字节序号），标识数据包在数据流中的位置
 * - ack: 确认号，表示期望接收的下一个字节序号（累积确认）
 * - len: 数据载荷长度（0-1024字节）
 * - checksum: 校验和，用于差错检测
 * - flags: 控制标志位（SYN/ACK/FIN/SACK）
 * - window_size: 接收窗口大小（单位：数据包），用于流量控制
 * - sack_count: SACK块数量（0-8）
 * - reserved: 保留字段，用于未来扩展
 */
struct RDTHeader {
    uint32_t seq;                   // 序列号（4字节）
    uint32_t ack;                   // 确认号（4字节）
    uint16_t len;                   // 数据长度（2字节）
    uint16_t checksum;              // 校验和（2字节）
    uint8_t  flags;                 // 控制标志（1字节）
    uint8_t  window_size;           // 接收窗口（1字节）
    uint8_t  sack_count;            // SACK块数量（1字节）
    uint8_t  reserved;              // 保留字段（1字节）
};                                  // 总计：12字节

/**
 * SACK块结构（8字节）
 * 功能：标识已接收的乱序数据范围
 * 设计思路：接收端可能收到不连续的数据包，通过SACK块告知发送端
 *          哪些数据已收到，避免不必要的重传
 * 示例：如果接收到seq=0, 2048, 3072，期望1024
 *      则SACK块为[2048, 4096)，表示2048-3071已收到
 */
struct SACKBlock {
    uint32_t start;                 // 起始序列号（包含）
    uint32_t end;                   // 结束序列号（不包含，左闭右开区间）
};

#pragma pack(pop)  // 恢复默认对齐方式

// ==================== 数据包结构定义 ====================

/**
 * 完整数据包结构
 * 包含：12字节头部 + 最多1024字节数据
 * 总大小：1036字节
 */
struct Packet {
    RDTHeader header;               // 协议头部
    char data[MSS];                 // 数据载荷（最大1024字节）
};

/**
 * SACK数据包结构
 * 用于发送带选择确认信息的ACK包
 * 包含：12字节头部 + 最多8个SACK块（每个8字节）
 * 总大小：12 + 8*8 = 76字节
 */
struct SACKPacket {
    RDTHeader header;               // 协议头部
    SACKBlock sack_blocks[MAX_SACK_BLOCKS];  // SACK块数组
};

// ==================== 性能统计结构 ====================
/**
 * 传输性能统计信息
 * 功能：记录传输过程中的关键性能指标
 * 用途：评估协议性能，分析窗口大小和丢包率的影响
 */
struct Statistics {
    uint64_t total_bytes;           // 总发送字节数
    uint64_t total_packets;         // 总发送数据包数
    uint64_t retransmitted_packets; // 重传数据包数（含超时和快速重传）
    uint64_t duplicate_acks;        // 收到的重复ACK数量
    uint64_t timeouts;              // 超时次数
    double transmission_time_ms;    // 传输总时间（毫秒）
    
    /**
     * 打印统计信息
     * 功能：计算并显示传输性能指标
     * 关键指标：吞吐率（Mbps）= 总字节数 * 8 / 传输时间（秒）
     */
    void print() {
        // 计算平均吞吐率（Mbps = Megabits per second）
        double throughput_mbps = (total_bytes * 8.0) / (transmission_time_ms * 1000.0);
        
        std::cout << "\n========== Transmission Statistics ==========" << std::endl;
        std::cout << "Total Bytes Sent: " << total_bytes << " bytes" << std::endl;
        std::cout << "Total Packets: " << total_packets << std::endl;
        std::cout << "Retransmitted Packets: " << retransmitted_packets << std::endl;
        std::cout << "Duplicate ACKs: " << duplicate_acks << std::endl;
        std::cout << "Timeouts: " << timeouts << std::endl;
        std::cout << "Transmission Time: " << transmission_time_ms / 1000.0 << " seconds" << std::endl;
        std::cout << "Average Throughput: " << throughput_mbps << " Mbps" << std::endl;
        std::cout << "=============================================" << std::endl;
    }
};

// ==================== 工具函数实现 ====================

/**
 * 计算16位Internet校验和（RFC 1071标准）
 * 
 * 算法原理：
 * 1. 将数据按16位（2字节）分组求和
 * 2. 如果有进位（超过16位），将进位加到低16位（反复执行直到无进位）
 * 3. 对结果取反码（按位取反）
 * 
 * 验证方法：
 * - 发送端：计算校验和并填入checksum字段
 * - 接收端：重新计算校验和，与接收到的校验和比较
 * - 如果相同，数据完整；否则数据损坏，丢弃数据包
 * 
 * @param data   数据包指针（包含RDTHeader）
 * @param length 数据包总长度（字节）
 * @return       16位校验和
 */
inline uint16_t calculate_checksum(void* data, size_t length) {
    uint32_t sum = 0;  // 使用32位累加器，捕获进位
    uint16_t* buf = reinterpret_cast<uint16_t*>(data);
    
    // 步骤1：保存并清零checksum字段（避免影响计算）
    uint16_t* checksum_ptr = &(reinterpret_cast<RDTHeader*>(data)->checksum);
    uint16_t old_checksum = *checksum_ptr;
    *checksum_ptr = 0;

    // 步骤2：按16位累加所有数据
    while (length > 1) {
        sum += *buf++;  // 累加16位
        length -= 2;    // 长度减2字节
    }

    // 步骤3：处理剩余的单字节（如果长度为奇数）
    if (length > 0) {
        sum += *reinterpret_cast<uint8_t*>(buf);  // 最后1字节补0作为16位
    }

    // 步骤4：将进位加到低16位（循环直到无进位）
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);  // 低16位 + 高16位（进位）
    }

    *checksum_ptr = old_checksum;  // 恢复原checksum值
    return static_cast<uint16_t>(~sum);  // 步骤5：取反码返回
}

/**
 * 模拟数据包丢失
 * 
 * 功能：根据配置的丢包率LOSS_RATE随机丢弃数据包
 * 实现：使用Mersenne Twister随机数生成器（高质量随机数）
 * 用途：测试协议在网络丢包情况下的可靠性和性能
 * 
 * @return true表示应该丢弃该包，false表示正常发送
 * 
 * 注意：控制包（SYN/ACK/FIN）通常不应丢弃，由调用者判断
 */
inline bool should_drop_packet() {
    static std::random_device rd;                       // 硬件随机数种子
    static std::mt19937 gen(rd());                      // Mersenne Twister引擎
    static std::uniform_real_distribution<> dis(0.0, 1.0);  // [0,1)均匀分布
    return dis(gen) < LOSS_RATE;  // 生成随机数并与丢包率比较
}

/**
 * 日志记录工具函数
 * 
 * 功能：格式化输出数据包信息，用于调试和监控
 * 输出格式：[前缀] Seq=xxx Ack=xxx Len=xxx Flags=SYN ACK Win=xxx
 * 
 * @param prefix   日志前缀（如"SEND"、"RECV"）
 * @param hdr      数据包头部
 * @param detailed 是否详细输出（false时不输出，避免刷屏）
 */
inline void log_packet(const char* prefix, const RDTHeader& hdr, bool detailed = false) {
    if (!detailed) return;  // 非详细模式直接返回
    
    std::cout << "[" << prefix << "] ";
    std::cout << "Seq=" << hdr.seq << " Ack=" << hdr.ack << " Len=" << hdr.len;
    std::cout << " Flags=";
    // 解析并输出所有设置的标志位
    if (hdr.flags & FLAG_SYN) std::cout << "SYN ";
    if (hdr.flags & FLAG_ACK) std::cout << "ACK ";
    if (hdr.flags & FLAG_FIN) std::cout << "FIN ";
    if (hdr.flags & FLAG_SACK) std::cout << "SACK ";
    std::cout << "Win=" << (int)hdr.window_size << std::endl;
}

#endif // RDT_PROTOCOL_H
