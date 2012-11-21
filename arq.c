#include "arq.h"

#define ACK_TIMEOUT 2           // Number of seconds to wait for an ACK
#define MAX_RESEND_ATTEMPTS 5   // Number of times to resend before failing.

static int sequence_number;
static int max_packet_size;

int arq_init(int loss_percentage, int max_packet_size_temp) {
    sequence_number = 0;

    if (set_dropper(loss_percentage) < 0) {
        return -1;
    }

    if (max_packet_size_temp <= 0) {
        max_packet_size = -1;
    } else {
        max_packet_size = max_packet_size_temp;
    }

    if (debug) {
        printf("Sequence Number: %d\n", sequence_number);
        printf("Loss Percentage: %d%%\n", loss_percentage);
        printf("Max Packet Size: %d byte(s)\n", max_packet_size);
    }

    return 1;
}

ssize_t arq_inform_send(int sock, struct sockaddr *dest_addr, int addr_len) {
    char buffer[BUFFER_MAX_SIZE];

    sprintf(buffer, "MPS %d", max_packet_size);

    return arq_sendto(sock, buffer, strlen(buffer), 0, dest_addr, addr_len);
}

ssize_t arq_sendto(int sock, void *buffer, size_t len, int flags, struct sockaddr *dest_addr, int addr_len) {
    struct timeval tv;
    time_t sent_time;
    int size = 0;
    char recv_buffer[BUFFER_MAX_SIZE];
    int recv_buffer_size;

    int message_received = 0;
    
    memset(recv_buffer, 0, BUFFER_MAX_SIZE);

    // Format message to send with sequence number
    char *package = malloc(sizeof(char) * BUFFER_MAX_SIZE);
    int package_size = message_encode(package, BUFFER_MAX_SIZE, sequence_number, buffer);

    // Send the message and see if we get an ACK
    int resend_attempt = 0;

    do {
        if (debug) {
            printf("Sending: %d %s\n", sequence_number, (char *) buffer);
        }
        
        size = sendto_dropper(sock, package, package_size, flags, dest_addr, addr_len);

        gettimeofday(&tv, 0);
        sent_time = tv.tv_sec;

        do {
            if ((recv_buffer_size = recvfrom(sock, recv_buffer, BUFFER_MAX_SIZE, MSG_DONTWAIT, 0, 0)) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Nothing bad happened, just nothing happened
                } else {
                    fprintf(stderr, "Could not receive message from server.");
                    exit(2);
                }
            } else {
                // We got a message!
                MESSAGE *message = message_decode(recv_buffer);

                int split_size = 0;
                char **split_buffer = split(message->message, " ", &split_size);

                if (split_size == 2 && strcmp(split_buffer[0], "ACK") == 0) {
                    // Received an ACK
                    int ack_sequence_number = atoi(split_buffer[1]);

                    if (debug) {
                        printf("Received: %d ACK %d\n", sequence_number, ack_sequence_number);
                    }

                    if (ack_sequence_number == sequence_number) {
                        message_received = 1;
                    }
                } else {
                    if (debug) {
                        printf("Received non-ACK message\n");
                    }
                }
            }

            gettimeofday(&tv, 0);
        } while (tv.tv_sec - sent_time < ACK_TIMEOUT && !message_received);

        if (!message_received) {
            resend_attempt++;
        }
    } while (!message_received && resend_attempt < MAX_RESEND_ATTEMPTS);

    if (resend_attempt >= MAX_RESEND_ATTEMPTS) {
        if (debug) {
            printf("Max number of resends exceeded. Giving up.\n");
        }

        return -1;
    }

    sequence_number = (sequence_number + 1) % 2;

    return size;
}

int arq_recvfrom(int sock, char *buffer, size_t len, int flags, struct sockaddr *src_addr, int *addr_len) {
    // Set up src_addr or addr_len if they are null (which is allowed by recvfrom usually)
    int src_addr_malloc = 0;
    int addr_len_malloc = 0;

    if (src_addr == 0) {
        src_addr = malloc(sizeof(struct sockaddr));
        src_addr_malloc = 1;
    }

    if (addr_len == 0) {
        addr_len = malloc(sizeof(int));
        addr_len_malloc = 1;

        *addr_len = sizeof(src_addr);
    }

    int size = recvfrom(sock, buffer, len, flags, src_addr, (socklen_t *) addr_len);

    MESSAGE *message = message_decode(buffer);

    if (debug) {
        printf("Received: %d %s\n", message->sequence_number, message->message);
    }

    // Respond with an ACK for the sequence number
    if ((arq_ack(sock, message->sequence_number, src_addr, *addr_len)) <= 0) {
        fprintf(stderr, "Could not send ACK.\n");
    }

    memset(buffer, 0, len);
    strcpy(buffer, message->message);

    // Free anything alloc'd
    if (src_addr_malloc) {
        free(src_addr);
    }

    if (addr_len_malloc) {
        free(addr_len);
    }
    
    // See if we've received a MPS message
    int split_size = 0;
    char **split_buffer = split(buffer, " ", &split_size);
    
    if (strcmp(split_buffer[0], "MPS") == 0) {
        max_packet_size = atoi(split_buffer[1]);

        if (debug) {
            printf("Updated max packet size to %d byte(s)\n", max_packet_size);
        }

        return arq_recvfrom(sock, buffer, len, flags, src_addr, addr_len);
    }
    
    return size;
}

ssize_t arq_ack(int sock, int sequence_number_ack, struct sockaddr *dest_addr, int addr_len) {
    char *message = malloc(sizeof(char) * BUFFER_MAX_SIZE);
    sprintf(message, "ACK %d", sequence_number_ack);

    char *package = malloc(sizeof(char) * BUFFER_MAX_SIZE);
    int package_size = message_encode(package, BUFFER_MAX_SIZE, sequence_number, message);

    int size = sendto_dropper(sock, package, package_size, 0, dest_addr, addr_len);

    if (debug) {
        printf("Sending: %d ACK %d\n", sequence_number, sequence_number_ack);
    }

    return size;
}
