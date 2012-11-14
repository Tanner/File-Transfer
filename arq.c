#include "arq.h"

#define ACK_TIMEOUT 2

static int sequence_number;

int arq_init(int loss_percentage) {
    sequence_number = 0;

    if (set_dropper(loss_percentage) < 0) {
        return -1;
    }

    if (debug) {
        printf("Sequence Number: %d\n", sequence_number);
        printf("Loss Percentage: %d\n", loss_percentage);
    }

    return 1;
}

ssize_t arq_sendto(int sock, void *buffer, size_t len, int flags, struct sockaddr *dest_addr, int addr_len) {
    struct timeval tv;
    time_t sent_time;
    int size;
    char recv_buffer[BUFFER_MAX_SIZE];
    int recv_buffer_size;

    int message_received = 0;
    
    memset(recv_buffer, 0, BUFFER_MAX_SIZE);

    // Format message to send with sequence number
    void *seq_buffer = malloc(sizeof(char) * BUFFER_MAX_SIZE);

    sprintf(seq_buffer, "%d ", sequence_number);
    strncat(seq_buffer, buffer, BUFFER_MAX_SIZE - strlen(seq_buffer));

    // Send the message and see if we get an ACK
    do {
        if (debug) {
            printf("Sending: %s\n", (char *) seq_buffer);
        }
        
        size = sendto_dropper(sock, seq_buffer, len, flags, dest_addr, addr_len);

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
                int split_size = 0;
                char **split_buffer = split(recv_buffer, " ", &split_size);

                if (split_size == 2 && strcmp(split_buffer[0], "ACK") == 0) {
                    int ack_sequence_number = atoi(split_buffer[1]);

                    if (ack_sequence_number == sequence_number) {
                        message_received = 1;
                    }
                }
            }

            gettimeofday(&tv, 0);
        } while (tv.tv_sec - sent_time < ACK_TIMEOUT && !message_received);
    } while (!message_received);

    sequence_number = (sequence_number + 1) % 2;

    return size;
}

ssize_t arq_recvfrom(int sock, char **buffer, size_t len, int flags, struct sockaddr *src_addr, int *addr_len) {
    int size = recvfrom(sock, *buffer, len, flags, src_addr, (socklen_t *) addr_len);

    if (debug) {
        printf("Received: %s\n", (char *) *buffer);
    }

    // Respond with an ACK for the sequence number
    int split_size = 0;
    char **split_buffer = split(*buffer, " ", &split_size);

    if (split_size <= 0) {
        fprintf(stderr, "Did not receive a sequence number.\n");
    }

    arq_ack(sock, atoi(split_buffer[0]), src_addr, *addr_len);

    // Strip out sequence number for buffer that the user will read
    *buffer = *buffer + sizeof(char) * 2;
    
    return size;
}

ssize_t arq_ack(int sock, int sequence_number, struct sockaddr *dest_addr, int addr_len) {
    char buffer[BUFFER_MAX_SIZE];

    sprintf(buffer, "ACK %d", sequence_number);

    int size = sendto_dropper(sock, buffer, strlen(buffer), 0, dest_addr, addr_len);

    if (debug) {
        printf("Sent ACK %d\n", sequence_number);
    }

    return size;
}
