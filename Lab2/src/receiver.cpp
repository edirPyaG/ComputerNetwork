#include "../include/rdt_protocol.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <vector>


using namespace std;

// Receiver State
SOCKET sock;
sockaddr_in server_addr, client_addr;
int client_addr_len = sizeof(client_addr);

// Buffer for out-of-order packets: <SequenceNumber, Packet>
map<uint32_t, Packet> packet_buffer;
uint32_t expected_seq = 0;   // Next expected sequence number
set<uint32_t> received_seqs; // Track all received sequences for SACK

// Statistics
uint64_t total_received = 0;
uint64_t total_acks_sent = 0;

void send_ack_with_sack(uint32_t cumulative_ack) {
  SACKPacket sack_pkt;
  memset(&sack_pkt, 0, sizeof(sack_pkt));

  sack_pkt.header.ack = cumulative_ack;
  sack_pkt.header.flags = FLAG_ACK | FLAG_SACK;

  // Flow Control: Advertise remaining buffer space
  int buffer_usage = packet_buffer.size();
  int available = RECV_WINDOW_SIZE - buffer_usage;
  sack_pkt.header.window_size = (available > 0) ? available : 0;

  // Build SACK blocks from received_seqs
  vector<SACKBlock> blocks;
  if (!received_seqs.empty()) {
    uint32_t start = *received_seqs.begin();
    uint32_t end = start;

    for (auto it = received_seqs.begin(); it != received_seqs.end(); ++it) {
      if (*it == end) {
        end = *it + MSS; // Assuming MSS-sized chunks
      } else {
        if (start >= cumulative_ack) {
          blocks.push_back({start, end});
        }
        start = *it;
        end = *it + MSS;
      }
    }
    if (start >= cumulative_ack && blocks.size() < MAX_SACK_BLOCKS) {
      blocks.push_back({start, end});
    }
  }

  sack_pkt.header.sack_count = min((int)blocks.size(), MAX_SACK_BLOCKS);
  for (int i = 0; i < sack_pkt.header.sack_count; i++) {
    sack_pkt.sack_blocks[i] = blocks[i];
  }

  size_t pkt_size =
      sizeof(RDTHeader) + sack_pkt.header.sack_count * sizeof(SACKBlock);
  sack_pkt.header.len = sack_pkt.header.sack_count * sizeof(SACKBlock);
  sack_pkt.header.checksum = calculate_checksum(&sack_pkt, pkt_size);

  // ACK packets should not be dropped (to avoid unnecessary retransmissions)
  sendto(sock, (char *)&sack_pkt, pkt_size, 0, (sockaddr *)&client_addr,
         client_addr_len);
  total_acks_sent++;

  log_packet("RECV-ACK", sack_pkt.header, false);
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  string local_ip = "0.0.0.0"; // Bind to all interfaces by default
  int local_port = DEFAULT_PORT;

  if (argc > 1) {
    int win = atoi(argv[1]);
    if (win > 0 && win <= 256) {
      RECV_WINDOW_SIZE = win;
    }
  }
  if (argc > 2)
    local_ip = argv[2];
  if (argc > 3)
    local_port = atoi(argv[3]);

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
  server_addr.sin_addr.s_addr =
      (local_ip == "0.0.0.0") ? INADDR_ANY : inet_addr(local_ip.c_str());
  server_addr.sin_port = htons(local_port);

  if (bind(sock, (sockaddr *)&server_addr, sizeof(server_addr)) ==
      SOCKET_ERROR) {
    cerr << "Bind failed on " << local_ip << ":" << local_port << endl;
    cerr << "Error code: " << WSAGetLastError() << endl;
    closesocket(sock);
    WSACleanup();
    return 1;
  }

  cout << "Receiver listening on " << local_ip << ":" << local_port << "..."
       << endl;

  ofstream outfile("received_file.dat", ios::binary);
  if (!outfile.is_open()) {
    cerr << "Failed to open output file" << endl;
    return 1;
  }

  Packet recv_pkt;
  auto start_time = clock();

  while (true) {
    int bytes_received = recvfrom(sock, (char *)&recv_pkt, sizeof(Packet), 0,
                                  (sockaddr *)&client_addr, &client_addr_len);
    if (bytes_received == SOCKET_ERROR)
      continue;

    // Verify Checksum
    size_t pkt_size = sizeof(RDTHeader) + recv_pkt.header.len;
    uint16_t received_checksum = recv_pkt.header.checksum;
    if (calculate_checksum(&recv_pkt, pkt_size) != received_checksum) {
      cout << "[Receiver] Checksum failed. Dropping packet." << endl;
      continue;
    }

    log_packet("RECV", recv_pkt.header, false);

    // Handle SYN
    if (recv_pkt.header.flags & FLAG_SYN) {
      cout << "[Receiver] SYN received. Initializing connection..." << endl;
      expected_seq = recv_pkt.header.seq + 1;

      Packet syn_ack;
      memset(&syn_ack, 0, sizeof(syn_ack));
      syn_ack.header.seq = 0;
      syn_ack.header.ack = expected_seq;
      syn_ack.header.flags = FLAG_SYN | FLAG_ACK;
      syn_ack.header.window_size = RECV_WINDOW_SIZE;
      syn_ack.header.checksum = calculate_checksum(&syn_ack, sizeof(RDTHeader));
      // SYN-ACK should not be dropped
      sendto(sock, (char *)&syn_ack, sizeof(RDTHeader), 0,
             (sockaddr *)&client_addr, client_addr_len);
      continue;
    }

    if (recv_pkt.header.flags & FLAG_FIN) {
      cout << "[Receiver] Step 1: FIN received from sender" << endl;

      cout << "[Receiver] Step 2: Sending ACK of FIN..." << endl;
      Packet ack_pkt;
      memset(&ack_pkt, 0, sizeof(ack_pkt));
      ack_pkt.header.ack = recv_pkt.header.seq + 1;
      ack_pkt.header.flags = FLAG_ACK;
      ack_pkt.header.checksum = calculate_checksum(&ack_pkt, sizeof(RDTHeader));
      sendto(sock, (char *)&ack_pkt, sizeof(RDTHeader), 0,
             (sockaddr *)&client_addr, client_addr_len);

      this_thread::sleep_for(chrono::milliseconds(100));

      cout << "[Receiver] Step 3: Sending FIN to sender..." << endl;
      Packet fin_pkt;
      memset(&fin_pkt, 0, sizeof(fin_pkt));
      fin_pkt.header.seq = expected_seq;
      fin_pkt.header.flags = FLAG_FIN;
      fin_pkt.header.checksum = calculate_checksum(&fin_pkt, sizeof(RDTHeader));
      sendto(sock, (char *)&fin_pkt, sizeof(RDTHeader), 0,
             (sockaddr *)&client_addr, client_addr_len);

      cout << "[Receiver] Step 4: Waiting for final ACK..." << endl;
      this_thread::sleep_for(chrono::milliseconds(200));

      cout << "[Receiver] 4-way handshake complete. Connection closed." << endl;
      break;
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
          Packet &buffered = packet_buffer[expected_seq];
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
