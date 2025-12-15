#include "../include/rdt_protocol.h"

// Global configuration variables
int SEND_WINDOW_SIZE = 64;      // Default sender window size (in packets)
int RECV_WINDOW_SIZE = 64;      // Default receiver window size (in packets)
double LOSS_RATE = 0.0;         // Default packet loss rate (0-1)
