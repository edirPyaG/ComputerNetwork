#ifndef RDT_STRUCTS_H
#define RDT_STRUCTS_H

#include <cstdint>
#include <cstring>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

// Protocol Constants
#define MSS 1024
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"

// Flags
#define FLAG_SYN 0x01
#define FLAG_ACK 0x02
#define FLAG_FIN 0x04

#pragma pack(push, 1)
struct RDTHeader {
    uint32_t seq;          // Sequence Number
    uint32_t ack;          // Acknowledgment Number
    uint16_t len;          // Length of data payload (exclude header)
    uint16_t checksum;     // 16-bit Internet Checksum
    uint8_t  flags;        // Control flags
    uint8_t  window_size;  // Receiver Window (rwnd) for Flow Control
};
#pragma pack(pop)

// Packet structure wrapper for convenience
struct Packet {
    RDTHeader header;
    char data[MSS];
};

// Checksum Calculation (Standard Internet Checksum)
// Algorithm: Sum all 16-bit words, add carry, then take 1's complement.
inline uint16_t calculate_checksum(Packet* pkt) {
    // Save current checksum
    uint16_t old_checksum = pkt->header.checksum;
    pkt->header.checksum = 0; // Zero out checksum field for calculation

    uint32_t sum = 0;
    uint16_t* buf = reinterpret_cast<uint16_t*>(pkt);
    size_t length = sizeof(RDTHeader) + pkt->header.len;

    while (length > 1) {
        sum += *buf++;
        length -= 2;
    }

    if (length > 0) {
        // Handle odd byte
        sum += *reinterpret_cast<uint8_t*>(buf);
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Restore old checksum (in case we are verifying)
    pkt->header.checksum = old_checksum;

    return static_cast<uint16_t>(~sum);
}

#endif // RDT_STRUCTS_H
