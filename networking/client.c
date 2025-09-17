#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include "sham.h"

// Struct for tracking sent packets in the sliding window
struct sent_packet_info {
    uint32_t seq_num;
    struct timeval sent_time;
    int data_len;
    struct sham_packet packet;
};

void die(char *s) {
    perror(s);
    exit(1);
}

// Helper to get time difference in milliseconds
long long timeval_diff_ms(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000LL + (end->tv_usec - start->tv_usec) / 1000LL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  File Transfer: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", argv[0]);
        fprintf(stderr, "  Chat Mode:     %s <server_ip> <server_port> --chat [loss_rate]\n", argv[0]);
        exit(1);
    }
    
    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int chat_mode = 0;
    char *input_file = NULL;
    char *output_file_name = NULL;

    // Argument Parsing
    if (argc > 3 && strcmp(argv[3], "--chat") == 0) {
        chat_mode = 1;
    } else if (argc > 4) {
        input_file = argv[3];
        output_file_name = argv[4];
    } else {
         fprintf(stderr, "Invalid arguments.\n");
         exit(1);
    }
    
    log_init("client_log.txt");

    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    struct sham_packet send_pkt, recv_pkt;
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) die("socket creation failed");
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_aton(server_ip, &server_addr.sin_addr) == 0) die("inet_aton() failed");

    // --- 3-Way Handshake ---
    uint32_t client_isn = rand() % 10000;
    
    memset(&send_pkt, 0, sizeof(send_pkt));
    send_pkt.header.seq_num = client_isn;
    send_pkt.header.flags = SYN_FLAG;
    sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    log_event("SND SYN SEQ=%u", client_isn);
    
    struct timeval timeout = {1, 0}; // 1 second timeout for handshake
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if(recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr*)&server_addr, &server_len) < 0) die("recvfrom (SYN-ACK) timeout");
    
    if ((recv_pkt.header.flags == (SYN_FLAG | ACK_FLAG)) && (recv_pkt.header.ack_num == client_isn + 1)) {
        log_event("RCV SYN-ACK SEQ=%u ACK=%u", recv_pkt.header.seq_num, recv_pkt.header.ack_num);
        
        memset(&send_pkt, 0, sizeof(send_pkt));
        send_pkt.header.flags = ACK_FLAG;
        send_pkt.header.seq_num = recv_pkt.header.ack_num;
        send_pkt.header.ack_num = recv_pkt.header.seq_num + 1;
        sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        log_event("SND ACK FOR SYN");
        printf("Connection established.\n");
    } else {
        die("Handshake failed");
    }

    uint32_t base_seq_num = send_pkt.header.seq_num;
    uint32_t next_seq_num = base_seq_num;
    
    timeout = (struct timeval){0, 0}; // Reset timeout to non-blocking for data transfer
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // --- Mode Selection ---
    if (chat_mode) {
        // --- CHAT MODE ---
        printf("Entering chat mode. Type '/quit' to exit.\n");
        fd_set read_fds;
        char send_buffer[PAYLOAD_SIZE];
        int max_fd = (sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO) + 1;
        int connection_active = 1;

        while (connection_active) {
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(sockfd, &read_fds);

            printf("> ");
            fflush(stdout);

            select(max_fd, &read_fds, NULL, NULL, NULL);

            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                if (fgets(send_buffer, sizeof(send_buffer), stdin)) {
                    if (strncmp(send_buffer, "/quit\n", 6) == 0) {
                        connection_active = 0;
                        continue;
                    }
                    memset(&send_pkt, 0, sizeof(send_pkt));
                    send_pkt.header.seq_num = next_seq_num;
                    memcpy(send_pkt.data, send_buffer, strlen(send_buffer) + 1);
                    sendto(sockfd, &send_pkt, HEADER_SIZE + strlen(send_buffer) + 1, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
                    log_event("SND DATA SEQ=%u LEN=%zu", next_seq_num, strlen(send_buffer));
                    next_seq_num += strlen(send_buffer);
                }
            }

            if (FD_ISSET(sockfd, &read_fds)) {
                int bytes = recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, NULL, NULL);
                if (bytes > 0) {
                    if (recv_pkt.header.flags & FIN_FLAG) {
                        log_event("RCV FIN SEQ=%u", recv_pkt.header.seq_num);
                        connection_active = 0;
                        continue;
                    }
                    if (bytes > HEADER_SIZE) {
                        printf("\rServer: %s> ", recv_pkt.data);
                        fflush(stdout);
                    }
                }
            }
        }
    } else {
        // --- FILE TRANSFER MODE ---
        sendto(sockfd, output_file_name, strlen(output_file_name) + 1, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        FILE *infile = fopen(input_file, "rb");
        if (!infile) die("Failed to open input file");

        struct sent_packet_info window[SENDER_WINDOW_SIZE];
        memset(window, 0, sizeof(window));
        
        uint32_t last_byte_acked = base_seq_num;
        uint16_t receiver_window = recv_pkt.header.window_size;
        int file_done = 0;

        while(!file_done || last_byte_acked < next_seq_num) {
            // Send new packets if window has space
            while((next_seq_num - last_byte_acked) < (SENDER_WINDOW_SIZE * PAYLOAD_SIZE) && 
                  (next_seq_num - last_byte_acked) < receiver_window && !file_done) {
                
                char buffer[PAYLOAD_SIZE];
                int bytes_read = fread(buffer, 1, PAYLOAD_SIZE, infile);
                
                if (bytes_read > 0) {
                    memset(&send_pkt, 0, sizeof(send_pkt));
                    send_pkt.header.seq_num = next_seq_num;
                    memcpy(send_pkt.data, buffer, bytes_read);
                    
                    int window_idx = (next_seq_num / PAYLOAD_SIZE) % SENDER_WINDOW_SIZE;
                    window[window_idx].seq_num = next_seq_num;
                    window[window_idx].data_len = bytes_read;
                    memcpy(&window[window_idx].packet, &send_pkt, sizeof(send_pkt));
                    gettimeofday(&window[window_idx].sent_time, NULL);

                    sendto(sockfd, &send_pkt, HEADER_SIZE + bytes_read, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
                    log_event("SND DATA SEQ=%u LEN=%d", next_seq_num, bytes_read);
                    next_seq_num += bytes_read;
                } else {
                    file_done = 1;
                }
            }

            // Check for incoming ACKs
            if (recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, NULL, NULL) > 0) {
                 if (recv_pkt.header.flags & ACK_FLAG) {
                    log_event("RCV ACK=%u WIN=%u", recv_pkt.header.ack_num, recv_pkt.header.window_size);
                    if (recv_pkt.header.ack_num > last_byte_acked) {
                        last_byte_acked = recv_pkt.header.ack_num;
                        receiver_window = recv_pkt.header.window_size;
                        log_event("FLOW WIN UPDATE=%u", receiver_window);
                    }
                 }
            }

            // Check for timeouts and retransmit
            struct timeval now;
            gettimeofday(&now, NULL);
            for (int i = 0; i < SENDER_WINDOW_SIZE; ++i) {
                if (window[i].seq_num != 0 && window[i].seq_num < next_seq_num && window[i].seq_num >= last_byte_acked) {
                    if (timeval_diff_ms(&window[i].sent_time, &now) > RTO_MS) {
                        log_event("TIMEOUT SEQ=%u", window[i].seq_num);
                        sendto(sockfd, &window[i].packet, HEADER_SIZE + window[i].data_len, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
                        log_event("RETX DATA SEQ=%u LEN=%d", window[i].seq_num, window[i].data_len);
                        gettimeofday(&window[i].sent_time, NULL); // Reset timer
                    }
                }
            }
            usleep(1000);
        }
        fclose(infile);
        printf("File transfer complete.\n");
    }

    // --- 4-Way Handshake (Termination) ---
    memset(&send_pkt, 0, sizeof(send_pkt));
    send_pkt.header.flags = FIN_FLAG;
    send_pkt.header.seq_num = next_seq_num;
    sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    log_event("SND FIN SEQ=%u", send_pkt.header.seq_num);

    timeout = (struct timeval){1, 0}; // 1 second timeout
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, NULL, NULL);
    log_event("RCV ACK FOR FIN");
    
    recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, NULL, NULL);
    if(recv_pkt.header.flags & FIN_FLAG) {
        log_event("RCV FIN SEQ=%u", recv_pkt.header.seq_num);
    
        memset(&send_pkt, 0, sizeof(send_pkt));
        send_pkt.header.flags = ACK_FLAG;
        send_pkt.header.ack_num = recv_pkt.header.seq_num + 1;
        sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        log_event("SND ACK=%u", send_pkt.header.ack_num);
    }
    
    printf("Connection closed.\n");
    close(sockfd);
    log_close();
    return 0;
}