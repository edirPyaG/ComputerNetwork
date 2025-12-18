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

// Protocol Constants
#define MSS 1024                    // Maximum Segment Size
#define DEFAULT_PORT 8888
#define DEFAULT_IP "127.0.0.1"
#define TIMEOUT_MS 200              // Retransmission timeout (lowered to cut RTO stalls)
#define MAX_SACK_BLOCKS 8           // Maximum SACK blocks

// Flags
#define FLAG_SYN 0x01
#define FLAG_ACK 0x02
#define FLAG_FIN 0x04
#define FLAG_SACK 0x08              // Selective ACK flag

// Configuration
extern int SEND_WINDOW_SIZE;        // Configurable via command line
extern int RECV_WINDOW_SIZE;
extern double LOSS_RATE;            // Packet loss simulation rate

#pragma pack(push, 1)
struct RDTHeader {
    uint32_t seq;                   // Sequence Number
    uint32_t ack;                   // Acknowledgment Number
    uint16_t len;                   // Length of data payload
    uint16_t checksum;              // 16-bit Internet Checksum
    uint8_t  flags;                 // Control flags
    uint8_t  window_size;           // Receiver Window (in packets)
    uint8_t  sack_count;            // Number of SACK blocks
    uint8_t  reserved;              // Reserved for future use
};

// SACK Block: Represents a range of received data
struct SACKBlock {
    uint32_t start;                 // Start sequence number
    uint32_t end;                   // End sequence number (exclusive)
};
#pragma pack(pop)

// Packet structure
struct Packet {
    RDTHeader header;
    char data[MSS];
};

// SACK Packet (ACK with selective acknowledgment blocks)
struct SACKPacket {
    RDTHeader header;
    SACKBlock sack_blocks[MAX_SACK_BLOCKS];
};

// Performance Statistics
struct Statistics {
    uint64_t total_bytes;
    uint64_t total_packets;
    uint64_t retransmitted_packets;
    uint64_t duplicate_acks;
    uint64_t timeouts;
    double transmission_time_ms;
    
    void print() {
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

// Checksum Calculation (Standard Internet Checksum)
inline uint16_t calculate_checksum(void* data, size_t length) {
    uint32_t sum = 0;
    uint16_t* buf = reinterpret_cast<uint16_t*>(data);
    
    // Save and zero out checksum field
    uint16_t* checksum_ptr = &(reinterpret_cast<RDTHeader*>(data)->checksum);
    uint16_t old_checksum = *checksum_ptr;
    *checksum_ptr = 0;

    while (length > 1) {
        sum += *buf++;
        length -= 2;
    }

    if (length > 0) {
        sum += *reinterpret_cast<uint8_t*>(buf);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    *checksum_ptr = old_checksum; // Restore
    return static_cast<uint16_t>(~sum);
}

// Packet loss simulation
inline bool should_drop_packet() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);
    return dis(gen) < LOSS_RATE;
}

// Logging utilities
inline void log_packet(const char* prefix, const RDTHeader& hdr, bool detailed = false) {
    if (!detailed) return;
    
    std::cout << "[" << prefix << "] ";
    std::cout << "Seq=" << hdr.seq << " Ack=" << hdr.ack << " Len=" << hdr.len;
    std::cout << " Flags=";
    if (hdr.flags & FLAG_SYN) std::cout << "SYN ";
    if (hdr.flags & FLAG_ACK) std::cout << "ACK ";
    if (hdr.flags & FLAG_FIN) std::cout << "FIN ";
    if (hdr.flags & FLAG_SACK) std::cout << "SACK ";
    std::cout << "Win=" << (int)hdr.window_size << std::endl;
}

#endif // RDT_PROTOCOL_H
