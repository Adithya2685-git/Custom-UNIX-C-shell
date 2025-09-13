#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#include <openssl/md5.h>
#include <openssl/evp.h>
#include "sham.h"

#define MAX_BUFFERED_PACKETS 50

struct buffered_packet {
    int valid;
    int data_len; // Store the actual payload length
    struct sham_packet packet;
};

void die(char *s) {
    perror(s);
    exit(1);
}

// MD5 calculation
void calculate_and_print_md5(const char *filename) {
    unsigned char c[MD5_DIGEST_LENGTH];
    FILE *inFile = fopen(filename, "rb");
    EVP_MD_CTX *mdctx;
    const EVP_MD *md;
    int bytes;
    unsigned char data[1024];
    unsigned int md_len;

    if (inFile == NULL) {
        printf("MD5: Error opening file\n");
        return;
    }

    md = EVP_get_digestbyname("MD5");
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);

    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        EVP_DigestUpdate(mdctx, data, bytes);
    }
    
    EVP_DigestFinal_ex(mdctx, c, &md_len);
    EVP_MD_CTX_free(mdctx);

    printf("MD5: ");
    for(int i = 0; i < md_len; i++) {
        printf("%02x", c[i]);
    }
    printf("\n");
    fclose(inFile);
}


int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <port> [--chat] [loss_rate]\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int chat_mode = 0;
    double loss_rate = 0.0;
    
    // Argument Parsing
    if (argc >= 3) {
        if (strcmp(argv[2], "--chat") == 0) {
            chat_mode = 1;
            if (argc == 4) loss_rate = atof(argv[3]);
        } else {
            loss_rate = atof(argv[2]);
        }
    }
    
    log_init("server_log.txt");

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct sham_packet recv_pkt, send_pkt;
    
    srand(time(NULL));

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) die("socket creation failed");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) die("bind failed");
    printf("Server listening on port %d\n", port);

    // --- 3-Way Handshake ---
    printf("Waiting for connection...\n");
    
    recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr*)&client_addr, &client_len);
    if (!(recv_pkt.header.flags & SYN_FLAG)) die("Handshake failed: Did not receive SYN");
    log_event("RCV SYN SEQ=%u", recv_pkt.header.seq_num);
    
    uint32_t server_isn = rand() % 10000;
    memset(&send_pkt, 0, sizeof(send_pkt));
    send_pkt.header.seq_num = server_isn;
    send_pkt.header.ack_num = recv_pkt.header.seq_num + 1;
    send_pkt.header.flags = SYN_FLAG | ACK_FLAG;
    send_pkt.header.window_size = RECEIVER_BUFFER_SIZE;
    sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&client_addr, client_len);
    log_event("SND SYN-ACK SEQ=%u ACK=%u", send_pkt.header.seq_num, send_pkt.header.ack_num);

    recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr*)&client_addr, &client_len);
    if (!((recv_pkt.header.flags & ACK_FLAG) && (recv_pkt.header.ack_num == server_isn + 1))) {
        die("Handshake failed: Invalid ACK for SYN-ACK");
    }
    log_event("RCV ACK FOR SYN");
    printf("Connection established with %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

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
            FD_SET(STDIN_FILENO, &read_fds); // Monitor keyboard input
            FD_SET(sockfd, &read_fds);      // Monitor network socket

            printf("> ");
            fflush(stdout);

            select(max_fd, &read_fds, NULL, NULL, NULL);

            // Check for keyboard input
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                if (fgets(send_buffer, sizeof(send_buffer), stdin)) {
                    if (strncmp(send_buffer, "/quit\n", 6) == 0) {
                        connection_active = 0; // Exit loop to start termination
                        continue;
                    }
                    memset(&send_pkt, 0, sizeof(send_pkt));
                    memcpy(send_pkt.data, send_buffer, strlen(send_buffer)+1);
                    sendto(sockfd, &send_pkt, HEADER_SIZE + strlen(send_buffer)+1, 0, (struct sockaddr*)&client_addr, client_len);
                    log_event("SND DATA (chat) LEN=%zu", strlen(send_buffer));
                }
            }

            // Check for network input
            if (FD_ISSET(sockfd, &read_fds)) {
                int bytes = recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr*)&client_addr, &client_len);
                if (bytes > 0) {
                    // If client initiated FIN
                    if (recv_pkt.header.flags & FIN_FLAG) {
                        log_event("RCV FIN SEQ=%u", recv_pkt.header.seq_num);
                        connection_active = 0;
                        continue; // Skip to termination
                    }
                    // If it's a data packet
                    if (bytes > HEADER_SIZE) {
                         printf("\rClient: %s> ", recv_pkt.data);
                         fflush(stdout);
                    }
                }
            }
        }
    } else {
        // --- FILE TRANSFER MODE ---
        char output_filename[256] = {0};
        recvfrom(sockfd, output_filename, sizeof(output_filename)-1, 0, (struct sockaddr*)&client_addr, &client_len);
        printf("Receiving file to be named: %s\n", output_filename);
        FILE *outfile = fopen(output_filename, "wb");
        if (!outfile) die("Failed to create output file");

        uint32_t expected_seq_num = recv_pkt.header.seq_num;
        struct buffered_packet buffer[MAX_BUFFERED_PACKETS];
        memset(buffer, 0, sizeof(buffer));

        while(1) {
            int bytes_received = recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr*)&client_addr, &client_len);
            if(bytes_received <= 0) continue;

            // Simulate packet loss (but not for ACKs or FINs)
            if ((double)rand() / RAND_MAX < loss_rate) {
                if (!(recv_pkt.header.flags & (ACK_FLAG | FIN_FLAG | SYN_FLAG))) {
                    log_event("DROP DATA SEQ=%u", recv_pkt.header.seq_num);
                    continue;
                }
            }
            
            // Check for FIN from client
            if (recv_pkt.header.flags & FIN_FLAG) {
                log_event("RCV FIN SEQ=%u", recv_pkt.header.seq_num);
                break; // Exit loop to handle termination
            }
            
            int payload_len = bytes_received - HEADER_SIZE;
            if (payload_len < 0) continue;
            log_event("RCV DATA SEQ=%u LEN=%d", recv_pkt.header.seq_num, payload_len);

            // Process in-order packets
            if (recv_pkt.header.seq_num == expected_seq_num) {
                if (payload_len > 0) fwrite(recv_pkt.data, 1, payload_len, outfile);
                expected_seq_num += payload_len;

                // Check buffer for next in-sequence packets
                int processed_from_buffer = 1;
                while (processed_from_buffer) {
                    processed_from_buffer = 0;
                    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
                        if (buffer[i].valid && buffer[i].packet.header.seq_num == expected_seq_num) {
                            if (buffer[i].data_len > 0) fwrite(buffer[i].packet.data, 1, buffer[i].data_len, outfile);
                            expected_seq_num += buffer[i].data_len;
                            buffer[i].valid = 0; // Invalidate buffer entry
                            processed_from_buffer = 1;
                        }
                    }
                }
            // Buffer out-of-order packets
            } else if (recv_pkt.header.seq_num > expected_seq_num) {
                // Avoid storing duplicates
                int already_buffered = 0;
                for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
                     if (buffer[i].valid && buffer[i].packet.header.seq_num == recv_pkt.header.seq_num) {
                        already_buffered = 1;
                        break;
                    }
                }
                if (!already_buffered) {
                    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
                        if (!buffer[i].valid) {
                            buffer[i].valid = 1;
                            memcpy(&buffer[i].packet, &recv_pkt, bytes_received);
                            buffer[i].data_len = payload_len;
                            break;
                        }
                    }
                }
            }
            
            // Send Cumulative ACK
            memset(&send_pkt, 0, sizeof(send_pkt));
            send_pkt.header.flags = ACK_FLAG;
            send_pkt.header.ack_num = expected_seq_num;
            send_pkt.header.window_size = RECEIVER_BUFFER_SIZE;
            sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&client_addr, client_len);
            log_event("SND ACK=%u WIN=%u", send_pkt.header.ack_num, send_pkt.header.window_size);
        }

        fclose(outfile);
        printf("File transfer complete.\n");
        calculate_and_print_md5(output_filename);
    }
    
    // --- 4-Way Handshake (Termination) ---
    memset(&send_pkt, 0, sizeof(send_pkt));
    send_pkt.header.flags = ACK_FLAG;
    sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&client_addr, client_len);
    log_event("SND ACK FOR FIN");
    
    memset(&send_pkt, 0, sizeof(send_pkt));
    send_pkt.header.flags = FIN_FLAG;
    send_pkt.header.seq_num = server_isn + 1;
    sendto(sockfd, &send_pkt, HEADER_SIZE, 0, (struct sockaddr*)&client_addr, client_len);
    log_event("SND FIN SEQ=%u", send_pkt.header.seq_num);

    recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr*)&client_addr, &client_len);
    if(recv_pkt.header.flags & ACK_FLAG) {
        log_event("RCV ACK=%u", recv_pkt.header.ack_num);
        printf("Connection closed.\n");
    }

    close(sockfd);
    log_close();
    return 0;
}