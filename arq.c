#include "arq.h"

#define ACK_TIMEOUT 2       // Number of seconds to wait for an ACK

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
    return arq_sendto_assist(sock, buffer, len, flags, dest_addr, addr_len, -1);
}

ssize_t arq_sendto_assist(int sock, void *buffer, size_t len, int flags, struct sockaddr *dest_addr, int addr_len, int messages_remaining) {
    struct timeval tv;
    time_t sent_time;
    int size = 0;
    char recv_buffer[BUFFER_MAX_SIZE];
    int recv_buffer_size;

    int message_received = 0;
    
    memset(recv_buffer, 0, BUFFER_MAX_SIZE);

    // Format message to send with sequence number and messages remaining
    void *seq_buffer = malloc(sizeof(char) * BUFFER_MAX_SIZE);

    sprintf(seq_buffer, "%d %d ", sequence_number, messages_remaining);

    if (messages_remaining == -1) {
        // Message has not been determined whether or not it needs to be broken up
        if (strlen(seq_buffer) + strlen(buffer) > max_packet_size) {
            // Split up the message
            int usable_size = max_packet_size - strlen(seq_buffer);

            int split_size = 0;
            char **split_buffer = arq_split_up_message(buffer, usable_size, &split_size);

            for (int i = 0; i < split_size; i++) {
                size += arq_sendto_assist(sock, split_buffer[i], strlen(split_buffer[i]), flags, dest_addr, addr_len, split_size - i - 1);
            }

            free(split_buffer);
            free(seq_buffer);

            return size;
        } else {
            // Don't need to split up the message
            messages_remaining = 0;

            memset(seq_buffer, 0, BUFFER_MAX_SIZE);
            sprintf(seq_buffer, "%d %d ", sequence_number, messages_remaining);
        }
    }

    strncat(seq_buffer, buffer, BUFFER_MAX_SIZE - strlen(seq_buffer));

    // Send the message and see if we get an ACK
    do {
        if (debug) {
            printf("Sending: %s\n", (char *) seq_buffer);
        }
        
        size = sendto_dropper(sock, seq_buffer, strlen(seq_buffer), flags, dest_addr, addr_len);

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

                if (split_size == 3 && strcmp(split_buffer[0], "ACK") == 0) {
                    // Received an ACK
                    int ack_sequence_number = atoi(split_buffer[1]);

                    if (ack_sequence_number == sequence_number) {
                        message_received = 1;
                    }
                } else {
                    // Did not receive an ACK
                    if (debug) {
                        printf("Received a message, but it wasn't an ACK\n");
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
    EXPECT *expect = arq_recvfrom_expect(sock, buffer, len, flags, src_addr, addr_len, 0);
    int size = expect->size;

    free(expect);

    // See if we've received a MPS message
    int split_size = 0;
    char **split_buffer = split(*buffer, " ", &split_size);
    
    if (strcmp(split_buffer[0], "MPS") == 0) {
        max_packet_size = atoi(split_buffer[1]);

        if (debug) {
            printf("Updated max packet size to %d byte(s)\n", max_packet_size);
        }

        return arq_recvfrom(sock, buffer, len, flags, src_addr, addr_len);
    }

    return size;
}

EXPECT * arq_recvfrom_expect(int sock, char **buffer, size_t len, int flags, struct sockaddr *src_addr, int *addr_len, int expect_handled) {
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

    int size = recvfrom(sock, *buffer, len, flags, src_addr, (socklen_t *) addr_len);

    if (debug) {
        printf("Received: %s\n", (char *) *buffer);
    }

    // Respond with an ACK for the sequence number
    int split_size = 0;
    char **split_buffer = split(*buffer, " ", &split_size);

    if (split_size < 3) {
        fprintf(stderr, "Did not receive a properly formatted message.\n");
    }

    int recv_sequence_number = atoi(split_buffer[0]);
    int recv_messages_remaining = atoi(split_buffer[1]);

    if ((arq_ack(sock, recv_sequence_number, src_addr, *addr_len)) < 0) {
        fprintf(stderr, "Could not send ACK.\n");
    }

    // Strip out sequence number and messages remaining from buffer that the user will read
    free(*buffer);

    *buffer = malloc(sizeof(char) * BUFFER_MAX_SIZE);
    memset(*buffer, 0, BUFFER_MAX_SIZE);

    for (int i = 2; i < split_size; i++) {
        if (i == 2) {
            sprintf(*buffer, "%s", split_buffer[i]);
        } else {
            sprintf(*buffer, "%s %s", *buffer, split_buffer[i]);
        }
    }

    // Create EXPECT struct
    EXPECT *expect = malloc(sizeof(EXPECT));
    expect->size = size;
    expect->messages_remaining = recv_messages_remaining;
    
    // Check to see if we need to handle more messages
    if (!expect_handled) {
        while (expect->messages_remaining > 0) {
            char *temp_buffer = malloc(sizeof(char) * BUFFER_MAX_SIZE);
            assert(temp_buffer);

            EXPECT *temp_expect = arq_recvfrom_expect(sock, &temp_buffer, BUFFER_MAX_SIZE, flags, src_addr, addr_len, 1);

            *buffer = realloc(*buffer, BUFFER_MAX_SIZE + BUFFER_MAX_SIZE);
            *buffer = memcpy(*buffer, temp_buffer, temp_expect->size);

            expect->size = temp_expect->size;
            expect->messages_remaining = temp_expect->messages_remaining;

            free(temp_expect);
        }
    }

    // Free anything alloc'd
    if (src_addr_malloc) {
        free(src_addr);
    }

    if (addr_len_malloc) {
        free(addr_len);
    }
    
    return expect;
}

ssize_t arq_ack(int sock, int sequence_number, struct sockaddr *dest_addr, int addr_len) {
    char buffer[BUFFER_MAX_SIZE];

    sprintf(buffer, "ACK %d 0", sequence_number);

    int size = sendto_dropper(sock, buffer, strlen(buffer), 0, dest_addr, addr_len);

    if (debug) {
        printf("Sent %s\n", buffer);
    }

    return size;
}

char ** arq_split_up_message(char *input, int chunk_size, int *size) {
    *size = ceil((double) strlen(input) / chunk_size);

    // Change the chunk_size and size appropriately if the size is no longer a single digit
    int num_digits = 1 + floor(log(*size) / log(10));

    if (num_digits > 1 && chunk_size > 1) {
        chunk_size -= (num_digits - 1);

        *size = ceil((double) strlen(input) / chunk_size);
    }

    char **split = calloc(*size, sizeof(char *));
    assert(split);

    for (int i = 0; i < *size; i++) {
        split[i] = malloc(sizeof(char) * chunk_size);
        assert(split[i]);

        split[i] = strncpy(split[i], input + chunk_size * i, chunk_size);
    }

    return split;
}
