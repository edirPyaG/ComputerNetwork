/*******************************************************************************
 * 文件名: receiver.cpp
 * 功能概述: RDT协议接收端实现
 * 
 * 整体架构设计:
 * 1. 单线程事件驱动模型:
 *    - 主线程循环接收数据包
 *    - 根据包类型（SYN/DATA/FIN）进行处理
 *    - 无需额外线程，逻辑简单清晰
 * 
 * 2. 乱序处理机制:
 *    - 顺序包：直接写入文件
 *    - 乱序包：缓存到packet_buffer，等待缺失数据
 *    - 缓存检查：每次写入后检查缓存中是否有连续数据
 * 
 * 3. SACK生成策略:
 *    - 追踪所有已接收的序列号（received_seqs）
 *    - 将连续的序列号合并为SACK块
 *    - 每次发送ACK时携带SACK信息（最多8块）
 * 
 * 4. 流量控制:
 *    - 计算剩余缓存空间 = RECV_WINDOW_SIZE - 当前缓存包数
 *    - 通过window_size字段通告发送端
 * 
 * 5. 差错检测:
 *    - 每个数据包均验证校验和
 *    - 校验失败的包直接丢弃
 * 
 * 关键数据结构:
 * - packet_buffer: 乱序数据包缓存区（map结构，按序列号索引）
 * - received_seqs: 已接收序列号集合（set结构，用于生成SACK）
 * - expected_seq: 期望接收的下一个序列号（累积确认）
 ******************************************************************************/

#include "../include/rdt_protocol.h"
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <ctime>

using namespace std;

// ==================== 全局状态变量 ====================

// Socket相关
SOCKET sock;                        // UDP套接字
sockaddr_in server_addr, client_addr;  // 服务器地址和客户端（发送端）地址
int client_addr_len = sizeof(client_addr);  // 客户端地址结构长度

// ==================== 缓存管理 ====================
// 乱序数据包缓存区: <序列号, 数据包>
map<uint32_t, Packet> packet_buffer;  // 使用map自动按序列号排序

uint32_t expected_seq = 0;           // 期望接收的下一个序列号（累积确认）
set<uint32_t> received_seqs;         // 已接收的所有序列号（用于生成SACK块）

// ==================== 统计信息 ====================
uint64_t total_received = 0;         // 总接收字节数
uint64_t total_acks_sent = 0;        // 总发送ACK数

// ==================== 核心函数实现 ====================

/**
 * 发送带SACK信息的ACK包
 * 
 * 功能: 构造并发送包含选择确认信息的ACK包
 * 
 * 设计思路:
 * 1. 设置累积确认号（cumulative_ack）
 * 2. 计算并通告接收窗口大小（流量控制）
 * 3. 从received_seqs中构造SACK块:
 *    - 遍历已接收的序列号
 *    - 将连续的序列号合并为一个SACK块
 *    - 只保留大于cumulative_ack的SACK块（已确认的不需要SACK）
 * 4. 计算校验和并发送
 * 
 * SACK块生成示例:
 * 已接收序列号: 0, 1024, 2048, 4096, 5120
 * expected_seq = 3072 (缺失3072)
 * 生成SACK块: [4096, 6144) 表示4096-5119已收到
 * 
 * @param cumulative_ack 累积确认号（期望接收的下一个序列号）
 */
void send_ack_with_sack(uint32_t cumulative_ack) {
    SACKPacket sack_pkt;
    memset(&sack_pkt, 0, sizeof(sack_pkt));  // 清零所有字段
    
    // ========== 步骤1: 设置基本头部信息 ==========
    sack_pkt.header.ack = cumulative_ack;         // 累积确认号
    sack_pkt.header.flags = FLAG_ACK | FLAG_SACK; // 设置ACK和SACK标志
    
    // ========== 步骤2: 流量控制 - 通告剩余缓存空间 ==========
    int buffer_usage = packet_buffer.size();        // 当前缓存的包数
    int available = RECV_WINDOW_SIZE - buffer_usage; // 剩余空间
    sack_pkt.header.window_size = (available > 0) ? available : 0;  // 避免负数
    
    // ========== 步骤3: 构造SACK块 ==========
    vector<SACKBlock> blocks;  // 临时存储SACK块
    
    if (!received_seqs.empty()) {
        // 初始化第一个SACK块
        uint32_t start = *received_seqs.begin();  // 最小的已接收序列号
        uint32_t end = start;                    // 当前块的结束序列号
        
        // 遍历所有已接收的序列号
        for (auto it = received_seqs.begin(); it != received_seqs.end(); ++it) {
            if (*it == end) {
                // 连续序列号，扩展当前块
                end = *it + MSS;  // 假设每个包大小MSS
            } else {
                // 不连续，关闭当前块，开始新块
                if (start >= cumulative_ack) {
                    // 只保留大于累积确认号的块（已确认的不需要SACK）
                    blocks.push_back({start, end});
                }
                start = *it;       // 开始新的SACK块
                end = *it + MSS;
            }
        }
        // 处理最后一个块
        if (start >= cumulative_ack && blocks.size() < MAX_SACK_BLOCKS) {
            blocks.push_back({start, end});
        }
    }
    
    // ========== 步骤4: 填充SACK块到数据包 ==========
    sack_pkt.header.sack_count = min((int)blocks.size(), MAX_SACK_BLOCKS);  // 最多8个块
    for (int i = 0; i < sack_pkt.header.sack_count; i++) {
        sack_pkt.sack_blocks[i] = blocks[i];
    }
    
    // ========== 步骤5: 计算数据包大小和校验和 ==========
    size_t pkt_size = sizeof(RDTHeader) + sack_pkt.header.sack_count * sizeof(SACKBlock);
    sack_pkt.header.len = sack_pkt.header.sack_count * sizeof(SACKBlock);  // 载荷长度
    sack_pkt.header.checksum = calculate_checksum(&sack_pkt, pkt_size);
    
    // ========== 步骤6: 发送ACK包 ==========
    // 注意: ACK包不应该丢弃，以减少不必要的重传
    sendto(sock, (char*)&sack_pkt, pkt_size, 0, (sockaddr*)&client_addr, client_addr_len);
    total_acks_sent++;  // 统计ACK数量
    
    log_packet("RECV-ACK", sack_pkt.header, false);  // 记录日志
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    string local_ip = "0.0.0.0";  // Bind to all interfaces by default
    int local_port = DEFAULT_PORT;
    
    if (argc > 1) {
        int win = atoi(argv[1]);
        if (win > 0 && win <= 256) {
            RECV_WINDOW_SIZE = win;
        }
    }
    if (argc > 2) local_ip = argv[2];
    if (argc > 3) local_port = atoi(argv[3]);
    
    // Validate window size
    if (RECV_WINDOW_SIZE <= 0) {
        cerr << "错误: 接收窗口大小必须 > 0! 当前值: " << RECV_WINDOW_SIZE << endl;
        return 1;
    }
    
    cout << "========== RDT Receiver ==========" << endl;
    cout << "Receiver Window Size: " << RECV_WINDOW_SIZE << " packets" << endl;
    cout << "Listen Address: " << local_ip << ":" << local_port << endl;
    cout << "==================================" << endl;
    
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return 1;
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Socket creation failed" << endl;
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = (local_ip == "0.0.0.0") ? INADDR_ANY : inet_addr(local_ip.c_str());
    server_addr.sin_port = htons(local_port);

    if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "Bind failed on " << local_ip << ":" << local_port << endl;
        cerr << "Error code: " << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Receiver listening on " << local_ip << ":" << local_port << "..." << endl;

    ofstream outfile("received_file.dat", ios::binary);
    if (!outfile.is_open()) {
        cerr << "Failed to open output file" << endl;
        return 1;
    }

    Packet recv_pkt;
    auto start_time = clock();
    
    while (true) {
        int bytes_received = recvfrom(sock, (char*)&recv_pkt, sizeof(Packet), 0, 
                                      (sockaddr*)&client_addr, &client_addr_len);
        if (bytes_received == SOCKET_ERROR) continue;

        // Verify Checksum
        size_t pkt_size = sizeof(RDTHeader) + recv_pkt.header.len;
        uint16_t received_checksum = recv_pkt.header.checksum;
        if (calculate_checksum(&recv_pkt, pkt_size) != received_checksum) {
            cout << "[Receiver] Checksum failed. Dropping packet." << endl;
            continue;
        }

        log_packet("RECV", recv_pkt.header, false);

        // ==================== 三次握手 - 接收端处理 ====================
        /**
         * TCP三次握手（Three-Way Handshake）在RDT协议中的实现
         * 
         * 三次握手是建立可靠连接的标准流程，目的是：
         * 1. 同步双方的初始序列号（ISN - Initial Sequence Number）
         * 2. 协商连接参数（如接收窗口大小）
         * 3. 确认双方都准备好进行数据传输
         * 
         * 握手流程图：
         * 
         * 发送端（Client）              接收端（Server）
         *    |                                |
         *    |-- 第1步：发送SYN (seq=0) ----->|  ← 接收端接收连接请求
         *    |                                |  - 发送端请求建立连接
         *    |                                |  - 发送自己的初始序列号seq=0
         *    |                                |
         *    |<-- 第2步：回复SYN-ACK ---------|  ← 此处实现【核心】
         *    |    (seq=0, ack=1)              |  - 接收端同意建立连接
         *    |                                |  - 发送自己的初始序列号seq=0
         *    |                                |  - 确认收到对方的SYN（ack=seq+1=1）
         *    |                                |  - 通告接收窗口大小（流量控制）
         *    |                                |
         *    |-- 第3步：发送ACK (ack=1) ----->|  ← 发送端确认建立连接
         *    |                                |  - 确认收到接收端的SYN-ACK
         *    |                                |  - 连接建立完成，可以开始传输数据
         *    |                                |
         *    [连接建立成功，开始数据传输]
         * 
         * 为什么需要三次握手？
         * - 两次不够：接收端无法确认发送端收到了SYN-ACK
         * - 三次刚好：双方都确认对方准备好了
         * - 防止旧连接请求：如果SYN包在网络中延迟到达，三次握手可以识别并拒绝
         * 
         * 关键字段说明：
         * - FLAG_SYN: 表示这是一个连接建立请求或响应
         * - seq: 序列号，用于标识数据包顺序（握手时为初始序列号）
         * - ack: 确认号，表示期望接收的下一个序列号
         * - window_size: 接收窗口大小，用于流量控制
         */
        if (recv_pkt.header.flags & FLAG_SYN) {
            cout << "[Receiver] SYN received. Initializing connection..." << endl;
            
            // ===== 步骤1: 初始化接收端的连接状态 =====
            // 根据收到的SYN包的序列号，计算期望接收的下一个序列号
            // 例如：收到SYN seq=0，则期望下一个数据包的序列号为1
            expected_seq = recv_pkt.header.seq + 1;  
            
            // ===== 步骤2: 构造SYN-ACK响应包 =====
            Packet syn_ack;
            memset(&syn_ack, 0, sizeof(syn_ack));  // 清零所有字段，避免脏数据
            
            // 设置我们（接收端）的初始序列号为0
            // 这是接收端的ISN（Initial Sequence Number）
            syn_ack.header.seq = 0;
            
            // 设置确认号，告诉发送端我们期望接收的下一个序列号
            // 这确认了我们收到了发送端的SYN包（seq=0），期望下一个seq=1
            syn_ack.header.ack = expected_seq;
            
            // 设置SYN和ACK标志
            // FLAG_SYN: 表示这是握手包，我们也在同步自己的序列号
            // FLAG_ACK: 表示这是确认包，确认收到了对方的SYN
            syn_ack.header.flags = FLAG_SYN | FLAG_ACK;
            
            // 通告接收窗口大小（流量控制）
            // 告诉发送端："我的缓存可以容纳这么多个数据包"
            // 发送端会根据这个值控制发送速率，避免接收端缓存溢出
            syn_ack.header.window_size = RECV_WINDOW_SIZE;
            
            // 计算校验和，用于差错检测
            // 接收端会验证这个校验和，确保数据在传输过程中没有被破坏
            syn_ack.header.checksum = calculate_checksum(&syn_ack, sizeof(RDTHeader));
            
            // ===== 步骤3: 发送SYN-ACK包 =====
            // 使用sendto()发送到发送端的地址（client_addr）
            // 注意：SYN-ACK是连接建立的关键包，在实际实现中不应该被随机丢弃
            sendto(sock, (char*)&syn_ack, sizeof(RDTHeader), 0, 
                   (sockaddr*)&client_addr, client_addr_len);
            
            cout << "[Receiver] SYN-ACK sent. Waiting for final ACK..." << endl;
            
            // ===== 步骤4: 继续等待 =====
            // 继续监听下一个包，可能是：
            // - 最终的ACK包（完成三次握手）
            // - 直接开始的数据包（隐含ACK，连接建立成功）
            continue;
        }

        // ==================== 四次挥手 - 接收端处理 ====================
        /**
         * 标准TCP四次挥手中接收端（被动关闭方）的角色：
         * 
         * 发送端(主动关闭)           接收端(被动关闭)
         *      |                            |
         *      |------ FIN (seq=X) -------->|  步骤1: 收到主动方的FIN ← 此处处理
         *      |                            |
         *      |<----- ACK (ack=X+1) -------|  步骤2: 回复ACK确认 ← 此处处理
         *      |                            |
         *      |<----- FIN (seq=Y) ---------|  步骤3: 发送自己的FIN ← 此处处理
         *      |                            |
         *      |------ ACK (ack=Y+1) ------>|  步骤4: 等待对方ACK
         *      |                            |
         * 
         * 处理逻辑：
         * 1. 收到对方FIN，先回复ACK（确认收到关闭请求）
         * 2. 完成数据处理后，发送自己的FIN（请求关闭自己这一方）
         * 3. 等待对方的ACK，然后关闭连接
         */
        if (recv_pkt.header.flags & FLAG_FIN) {
            cout << "[Receiver] Step 1: FIN received from sender" << endl;
            
            // ----- 步骤2: 立即回复ACK，确认收到FIN -----
            cout << "[Receiver] Step 2: Sending ACK of FIN..." << endl;
            Packet ack_pkt;
            memset(&ack_pkt, 0, sizeof(ack_pkt));
            ack_pkt.header.ack = recv_pkt.header.seq + 1;  // 确认对方的FIN
            ack_pkt.header.flags = FLAG_ACK;               // 只有ACK标志
            ack_pkt.header.checksum = calculate_checksum(&ack_pkt, sizeof(RDTHeader));
            sendto(sock, (char*)&ack_pkt, sizeof(RDTHeader), 0, 
                   (sockaddr*)&client_addr, client_addr_len);
            
            // 短暂等待，模拟处理剩余数据的时间
            this_thread::sleep_for(chrono::milliseconds(100));
            
            // ----- 步骤3: 发送自己的FIN，请求关闭 -----
            cout << "[Receiver] Step 3: Sending FIN to sender..." << endl;
            Packet fin_pkt;
            memset(&fin_pkt, 0, sizeof(fin_pkt));
            fin_pkt.header.seq = expected_seq;             // 使用当前序列号
            fin_pkt.header.flags = FLAG_FIN;               // 只有FIN标志
            fin_pkt.header.checksum = calculate_checksum(&fin_pkt, sizeof(RDTHeader));
            sendto(sock, (char*)&fin_pkt, sizeof(RDTHeader), 0, 
                   (sockaddr*)&client_addr, client_addr_len);
            
            // ----- 步骤4: 等待对方的ACK（可选，简化处理）-----
            // 在实际TCP中，这里会等待对方的ACK，并有TIME_WAIT状态
            // 为了简化，这里直接关闭
            cout << "[Receiver] Step 4: Waiting for final ACK..." << endl;
            this_thread::sleep_for(chrono::milliseconds(200));
            
            cout << "[Receiver] 4-way handshake complete. Connection closed." << endl;
            break;  // 退出while(true)循环，结束接收
        }

        // Data Processing
        uint32_t seq = recv_pkt.header.seq;
        uint16_t len = recv_pkt.header.len;

        if (len > 0) {
            total_received += len;
            received_seqs.insert(seq);
            
            if (seq == expected_seq) {
                // In-order packet
                outfile.write(recv_pkt.data, len);
                expected_seq += len;

                // Check buffer for consecutive packets
                while (packet_buffer.count(expected_seq)) {
                    Packet& buffered = packet_buffer[expected_seq];
                    outfile.write(buffered.data, buffered.header.len);
                    expected_seq += buffered.header.len;
                    packet_buffer.erase(expected_seq - buffered.header.len);
                }
                
                // Clean up received_seqs
                auto it = received_seqs.begin();
                while (it != received_seqs.end() && *it < expected_seq) {
                    it = received_seqs.erase(it);
                }
            } else if (seq > expected_seq) {
                // Out-of-order: buffer it
                if (packet_buffer.size() < (size_t)RECV_WINDOW_SIZE) {
                    packet_buffer[seq] = recv_pkt;
                }
            }
            
            // Send SACK
            send_ack_with_sack(expected_seq);
        }
    }

    auto end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    cout << "\n========== Receiver Statistics ==========" << endl;
    cout << "Total Bytes Received: " << total_received << " bytes" << endl;
    cout << "Total ACKs Sent: " << total_acks_sent << endl;
    cout << "Reception Time: " << elapsed << " seconds" << endl;
    cout << "=========================================" << endl;

    outfile.close();
    closesocket(sock);
    WSACleanup();
    return 0;
}
