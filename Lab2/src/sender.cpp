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

// Sender State
SOCKET sock;
sockaddr_in server_addr;

// Congestion Control & Flow Control
atomic<uint32_t> cwnd(MSS);
atomic<uint32_t> ssthresh(65535);
atomic<uint32_t> rwnd(65535);
atomic<uint32_t> dupACKcount(0);
atomic<uint32_t> last_ack(0);

// Sequence Numbers
atomic<uint32_t> base(0);
atomic<uint32_t> next_seq_num(0);
atomic<bool> connection_active(true);

// NewReno State
atomic<uint32_t> recover(0);
atomic<bool> in_fast_recovery(false);

// Send Buffer
struct SentPacket {
    Packet pkt;
    chrono::steady_clock::time_point time_sent;
    bool acked;
};
vector<SentPacket> send_buffer;
mutex buffer_mutex;

// Statistics
Statistics stats = {0};
chrono::steady_clock::time_point transmission_start;
bool verbose = false;

void send_packet(Packet& pkt) {
    size_t pkt_size = sizeof(RDTHeader) + pkt.header.len;
    pkt.header.checksum = calculate_checksum(&pkt, pkt_size);
    
    // Simulate packet loss for data packets only (not control packets)
    // Control packets (SYN, ACK, FIN) should always be sent
    bool is_control_packet = (pkt.header.flags & (FLAG_SYN | FLAG_ACK | FLAG_FIN)) != 0;
    bool is_data_packet = pkt.header.len > 0;
    
    if (is_control_packet || !is_data_packet || !should_drop_packet()) {
        sendto(sock, (char*)&pkt, static_cast<int>(pkt_size), 0, (sockaddr*)&server_addr, sizeof(server_addr));
    } else {
        // cout << "[Sender] Simulated packet loss at seq=" << pkt.header.seq << endl;
    }
    
    log_packet("SEND", pkt.header, verbose);
}

void retransmit_packet(uint32_t seq_num) {
    for (auto& item : send_buffer) {
        if (item.pkt.header.seq == seq_num && !item.acked) {
            cout << "[Sender] Retransmitting seq=" << seq_num << endl;
            send_packet(item.pkt);
            item.time_sent = chrono::steady_clock::now();
            stats.retransmitted_packets++;
            return;
        }
    }
}

void timer_thread_func() {
    while (connection_active) {
        this_thread::sleep_for(chrono::milliseconds(10));
        
        lock_guard<mutex> lock(buffer_mutex);
        if (send_buffer.empty()) continue;

        auto now = chrono::steady_clock::now();
        auto& oldest = send_buffer.front();
        
        if (!oldest.acked && 
            chrono::duration_cast<chrono::milliseconds>(now - oldest.time_sent).count() > TIMEOUT_MS) {
            
            cout << "[Sender] Timeout! seq=" << oldest.pkt.header.seq  << " cwnd=" << cwnd << endl;
            
            // Timeout: ssthresh = cwnd/2, cwnd = MSS
            ssthresh = max((uint32_t)MSS, (uint32_t)cwnd / 2);
            cwnd = 4 * MSS; // Optimization: Start with larger window after timeout
            dupACKcount = 0;
            in_fast_recovery = false; // Reset NewReno state on timeout
            
            // cout << cwnd << endl;
            
            // send_packet(oldest.pkt); // Only sending the oldest might not be enough if pipe is empty, but standard Reno says Retransmit First Unacked.
            
            // CRITICAL FIX: To prevent domino timeouts for subsequent packets in the window:
            // When a timeout occurs, we must assume the network state has changed or is unknown.
            // We reset the timers for ALL outstanding packets to prevent them from timing out 
            // instantly one after another as they reach the front of the queue.
            send_packet(oldest.pkt);
            stats.retransmitted_packets++;
            
            for (auto& item : send_buffer) {
                item.time_sent = now;
            }
            // oldest.time_sent = now; // Handled by loop above
            
            stats.timeouts++;
        }
    }
}

void listener_thread() {
    SACKPacket sack_pkt;
    
    while (connection_active) {
        int bytes = recvfrom(sock, (char*)&sack_pkt, sizeof(SACKPacket), 0, NULL, NULL);
        if (bytes == SOCKET_ERROR) continue;

        size_t expected_size = sizeof(RDTHeader) + sack_pkt.header.len;
        if (calculate_checksum(&sack_pkt, expected_size) != sack_pkt.header.checksum) {
            continue;
        }

        uint32_t ack = sack_pkt.header.ack;
        rwnd = (uint32_t)sack_pkt.header.window_size * MSS;

        log_packet("RECV-ACK", sack_pkt.header, verbose);

        lock_guard<mutex> lock(buffer_mutex);
        
        // Handle SACK blocks
        if (sack_pkt.header.flags & FLAG_SACK) {
            for (int i = 0; i < sack_pkt.header.sack_count; i++) {
                uint32_t sack_start = sack_pkt.sack_blocks[i].start;
                uint32_t sack_end = sack_pkt.sack_blocks[i].end;
                
                // Mark packets in SACK range as acked
                for (auto& item : send_buffer) {
                    if (item.pkt.header.seq >= sack_start && 
                        item.pkt.header.seq < sack_end) {
                        item.acked = true;
                    }
                }
            }
        }
        
        if (ack > base) {
            // New ACK
            if (verbose) {
                cout << "[Sender] New ACK=" << ack << " cwnd=" << cwnd;
            }
            
            // Remove acked packets from buffer
            {
                // We use a temporary lock if strictly needed, but careful about lock order.
                // Assuming buffer_mutex is already locked by the caller (Wait, listener_thread locks it at line 122)
                // Yes, buffer_mutex is locked from line 122 in original code.
                // Note: The original code context for this replacement is inside listener_thread logic.
                // However, listener_thread locks buffer_mutex at line 122. This block starts around line 140.
                // So we are inside the lock.
                
                while (!send_buffer.empty()) {
                    uint32_t pkt_end = send_buffer.front().pkt.header.seq + 
                                       send_buffer.front().pkt.header.len;
                    if (pkt_end <= ack) {
                        send_buffer.erase(send_buffer.begin());
                    } else {
                        break;
                    }
                }
                
                // CRITICAL FIX (RFC 6298): Restart retransmission timer on New ACK
                // Give the new oldest packet a fresh timeout window.
                if (!send_buffer.empty()) {
                    send_buffer.front().time_sent = chrono::steady_clock::now();
                }
            }

            // NewReno Logic
            if (in_fast_recovery) {
                if (ack >= recover) {
                    // Full ACK: Covers all data sent before Fast Recovery initiated
                    if (verbose) cout << " -> (Full ACK, Exit Fast Recovery)" << endl;
                    
                    in_fast_recovery = false;
                    dupACKcount = 0;
                    
                    // Exit Fast Recovery: set cwnd to ssthresh (or FlightSize)
                    cwnd = ssthresh.load(); 
                } else {
                    // Partial ACK: Covers new data, but not all data up to 'recover'
                    if (verbose) cout << " -> (Partial ACK, Stay in Fast Recovery)" << endl;
                    
                    // Improvements for Partial ACK:
                    // 1. Deflate cwnd by amount of new data acknowledged? (Optional, kept simple here to maintain throughput)
                    // 2. IMMEDIATE RETRANSMIT of the next missing segment (which starts at 'ack')
                    //    This is the key fix for NewReno.
                    cout << "[Sender] Partial ACK received. Retransmitting seq=" << ack << endl;
                    retransmit_packet(ack);
                    
                    // Do NOT exit Fast Recovery
                    // Do NOT reset dupACKcount logic effectively
                }
            } else {
                // Standard Reno (Not in Fast Recovery)
                base = ack;
                last_ack = ack;
                dupACKcount = 0;

                // Congestion Control: Slow Start / Congestion Avoidance
                if (cwnd < ssthresh) {
                    cwnd += MSS; // Slow Start
                    if (verbose) cout << " -> " << cwnd << " (Slow Start)" << endl;
                } else {
                    uint32_t increment = max((uint32_t)1, MSS * MSS / cwnd);
                    cwnd += increment; // Congestion Avoidance
                    if (verbose) cout << " -> " << cwnd << " (Congestion Avoidance)" << endl;
                }
            }
            
            base = ack; // Ensure base is updated
            last_ack = ack;

        } else if (ack == last_ack) {
            // Duplicate ACK
            dupACKcount++;
            stats.duplicate_acks++;
            
            if (dupACKcount == 3) {
                // Fast Retransmit & Enter Fast Recovery
                cout << "[Sender] 2 Dup ACKs! Fast Retransmit seq=" << ack 
                     << " cwnd=" << cwnd << " -> ";
                
                recover = next_seq_num.load(); // Record the recovery point (NewReno)
                in_fast_recovery = true;
                
                ssthresh = max((uint32_t)MSS, (uint32_t)cwnd / 2);
                cwnd = ssthresh.load() + 3 * MSS;
                
                cout << cwnd << endl;
                
                retransmit_packet(ack);
            } else if (dupACKcount > 3) {
                // Fast Recovery: inflate cwnd
                cwnd += MSS;
            }
        }
        
        if (sack_pkt.header.flags & FLAG_FIN) {
            cout << "[Sender] FIN-ACK received. Connection closed." << endl;
            connection_active = false;
            break;
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
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(target_ip.c_str());
    server_addr.sin_port = htons(target_port);

    // 3-Way Handshake
    cout << "[Sender] Starting handshake..." << endl;
    Packet syn_pkt;
    memset(&syn_pkt, 0, sizeof(syn_pkt));
    syn_pkt.header.seq = 0;
    syn_pkt.header.flags = FLAG_SYN;
    send_packet(syn_pkt);
    
    Packet syn_ack;
    int len = sizeof(server_addr);
    // Handshake Timeout Handling
    auto handshake_start = chrono::steady_clock::now();
    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_MS * 1000; // 300ms
        
        int activity = select(0, &read_fds, NULL, NULL, &timeout);
        
        if (activity > 0) {
            int res = recvfrom(sock, (char*)&syn_ack, sizeof(Packet), 0, 
                              (sockaddr*)&server_addr, &len);
            if (res > 0 && (syn_ack.header.flags & (FLAG_SYN | FLAG_ACK)) && 
                syn_ack.header.ack == 1) {
                cout << "[Sender] Handshake complete." << endl;
                base = 1;
                next_seq_num = 1;
                rwnd = syn_ack.header.window_size * MSS;
                break;
            }
        } else {
            // Timeout reached, retransmit SYN
            cout << "[Sender] Handshake timeout. Retransmitting SYN..." << endl;
            send_packet(syn_pkt);
        }
    }
    
    // Send final ACK
    Packet ack_pkt;
    memset(&ack_pkt, 0, sizeof(ack_pkt));
    ack_pkt.header.seq = 1;
    ack_pkt.header.ack = syn_ack.header.seq + 1;
    ack_pkt.header.flags = FLAG_ACK;
    send_packet(ack_pkt);

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

    // Start transmission
    transmission_start = chrono::steady_clock::now();
    char buffer[MSS];
    
    while (infile.read(buffer, MSS) || infile.gcount() > 0) {
        int bytes_read = static_cast<int>(infile.gcount());
        
        // Flow & Congestion Control
        while (true) {
            uint32_t inflight = next_seq_num - base;
            uint32_t win = min((uint32_t)SEND_WINDOW_SIZE * MSS, min((uint32_t)cwnd, (uint32_t)rwnd));
            if (inflight < win) break;
            this_thread::yield(); // Better than sleep for high throughput
        }

        Packet data_pkt;
        memset(&data_pkt, 0, sizeof(data_pkt));
        data_pkt.header.seq = next_seq_num;
        data_pkt.header.len = bytes_read;
        memcpy(data_pkt.data, buffer, bytes_read);

        {
            lock_guard<mutex> lock(buffer_mutex);
            send_packet(data_pkt);
            send_buffer.push_back({data_pkt, chrono::steady_clock::now(), false});
        }
        
        // PACING: Removed 500us sleep to restore throughput
        // this_thread::sleep_for(chrono::microseconds(500));
        
        stats.total_packets++;
        stats.total_bytes += bytes_read;
        next_seq_num += bytes_read;
    }

    // Wait for all ACKs
    cout << "[Sender] Waiting for all ACKs..." << endl;
    while (base < next_seq_num) {
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    auto transmission_end = chrono::steady_clock::now();
    stats.transmission_time_ms = static_cast<double>(chrono::duration_cast<chrono::milliseconds>(
        transmission_end - transmission_start).count());

    
    cout << "[Sender] Starting 4-way handshake to close connection..." << endl;
    

    cout << "[Sender] Step 1: Sending FIN..." << endl;
    Packet fin_pkt;
    memset(&fin_pkt, 0, sizeof(fin_pkt));
    fin_pkt.header.seq = next_seq_num;
    fin_pkt.header.flags = FLAG_FIN;
    send_packet(fin_pkt);

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

    this_thread::sleep_for(chrono::milliseconds(200));
    connection_active = false;

    stats.print();

    closesocket(sock);
    WSACleanup();
    return 0;
}
