# Project Specification: Reliable Data Transport (RDT) Protocol over UDP
# 实验指导文档：基于UDP的可靠传输协议设计与实现

## 1. Overview (项目概况)
**Goal:** Implement a connection-oriented, reliable data transmission protocol over UDP in User Space (Application Layer).
**Language:** C++ (C++11/14/17 standard preferred).
**Libraries:** Standard POSIX Sockets (`<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`) or Winsock2. **NO** wrapper classes like `CSocket`, `QtNetwork`, or `Boost.asio`.
**Deadline:** 2025-12-27.
**Key Features:** Connection Management, Checksum, Selective Repeat (SR), Flow Control, TCP RENO Congestion Control.

---

## 2. Architecture Design (架构设计)

### 2.1 Threading Model (Sender)
Since standard `recvfrom` is blocking, the Sender must be multi-threaded to handle ACKs while processing application data.
* **Main Thread (Data Producer):** * Reads data from file.
    * Checks `min(cwnd, rwnd)` to see if sending is allowed.
    * Packages data into `Packet` and calls `sendto_simulated`.
    * Manages the "Send Buffer" (adding new packets).
* **Listener Thread (ACK Consumer):** * Runs in an infinite loop calling `recvfrom`.
    * Processes incoming ACKs.
    * Updates `cwnd`, `ssthresh`, and handles `dupACKcount` (RENO logic).
    * Removes acknowledged packets from the "Send Buffer".

### 2.2 Receiver Design
* **Single Thread:** Blocking loop on `recvfrom`.
* **Logic:** Receive packet -> Verify Checksum -> Check Sequence Number -> Buffer (if out-of-order) or Write to File (if in-order) -> Send ACK.

---

## 3. Protocol Data Structures (协议数据结构)

**Requirement:** Use strict byte alignment (`#pragma pack(1)`) to ensure header consistency across network.

### 3.1 Header Definition
```cpp
#include <cstdint>

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

// Flags
#define FLAG_SYN 0x01
#define FLAG_ACK 0x02
#define FLAG_FIN 0x04
// #define FLAG_RST 0x08 // Optional

// Maximum Segment Size (Data only)
#define MSS 1024