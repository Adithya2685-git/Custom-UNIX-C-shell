#ifndef SHAM_H
#define SHAM_H

#include <stdint.h>
#include <stdio.h>
#include<stdlib.h>
// Control Flags
#define SYN_FLAG 0x1
#define ACK_FLAG 0x2
#define FIN_FLAG 0x4

// Constants
#define MAX_PACKET_SIZE 1400 
#define HEADER_SIZE sizeof(struct sham_header)
#define PAYLOAD_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)
#define SENDER_WINDOW_SIZE 10 
#define RTO_MS 500 // Retransmission Timeout in milliseconds
#define RECEIVER_BUFFER_SIZE (SENDER_WINDOW_SIZE * PAYLOAD_SIZE * 2) // Receiver can buffer twice the window size

// S.H.A.M. Packet Structure
struct sham_header {
    uint32_t seq_num;     // Sequence Number
    uint32_t ack_num;     // Acknowledgment Number
    uint16_t flags;       // Control flags (SYN, ACK, FIN)
    uint16_t window_size; // Flow control window size (receiver's available buffer)
} __attribute__((packed));

struct sham_packet {
    struct sham_header header;
    char data[PAYLOAD_SIZE];
};

void log_init(const char* filename);
void log_event(const char* format, ...);
void log_close();

extern FILE* log_file; // Global log file pointer
extern int logging_enabled;

#endif