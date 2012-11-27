#include "arq.h"

#define ACK_TIMEOUT 50           // Number of ms to wait for an ACK
#define MAX_RESEND_ATTEMPTS 10   // Number of times to resend before failing.

static int sequence_number;
static int max_packet_size;

/**
 * Set up the ARQ with the Max Packet Size.
 * @param loss_percentage       Loss percentage from 0 to 100 percent
 * @param max_packet_size_temp  Max Packet Size
 *
 * @return Success value, -1 for failure
 */
int arq_init(int loss_percentage, int max_packet_size_temp) {
    sequence_number = 0;

    if (set_dropper(loss_percentage) < 0) {
        return -1;
    }

    // Ensure the MPS gets set correctly
    if (max_packet_size_temp <= 0) {
        max_packet_size = -1;
    } else if (max_packet_size_temp < MIN_PACKET_SIZE) {
        max_packet_size = MIN_PACKET_SIZE;
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

/**
 * Inform the server of the MPS.
 * @param sock          Socket to use
 * @param dest_addr     Destination Address
 * @param addr_len      Size of dest_addr
 */
ssize_t arq_inform_send(int sock, struct sockaddr *dest_addr, int addr_len) {
    char buffer[arq_get_max_packet_size()];
    memset(buffer, 0, arq_get_max_packet_size());

    sprintf(buffer, "MPS %d", max_packet_size);

    return arq_sendto(sock, buffer, strlen(buffer), 0, dest_addr, addr_len);
}

/**
 * Sends a message to the someone.
 * @param sock          Socket to use
 * @param buffer        Buffer to send
 * @param len           Length of buffer
 * @param flags         Flags
 * @param dest_addr     Destination Address
 * @param addr_len      Size of dest_addr
 *
 * @return Sent data size
 */
ssize_t arq_sendto(int sock, void *buffer, size_t len, int flags, struct sockaddr *dest_addr, int addr_len) {
    struct timeval tv;
    long int sent_time, current_time;
    int size = 0;
    char recv_buffer[arq_get_max_packet_size()];
    int recv_buffer_size;

    int message_received = 0;
    
    memset(recv_buffer, 0, arq_get_max_packet_size());

    // Format message to send with sequence number
    char *package = calloc(1, sizeof(char) * arq_get_max_packet_size());
    int package_size = message_encode(package, arq_get_max_packet_size(), sequence_number, buffer, len);

    // Send the message and see if we get an ACK
    int resend_attempt = 0;

    do {
        // Non-blocking receive until max number of attempts exceeded
        if (debug) {
            printf("Sending: #%d <%s> (%d bytes)\n", sequence_number, (char *) buffer, len);
        }
        
        size = sendto_dropper(sock, package, package_size, flags, dest_addr, addr_len);

        gettimeofday(&tv, 0);
        sent_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

        do {
            // Non-blocking receive until time out expired to send again
            if ((recv_buffer_size = recvfrom(sock, recv_buffer, arq_get_max_packet_size(), MSG_DONTWAIT, 0, 0)) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Nothing bad happened, just nothing happened
                } else {
                    fprintf(stderr, "Could not receive message from server.");
                    exit(2);
                }
            } else {
                // We got a message!
                MESSAGE *message = message_decode(recv_buffer, recv_buffer_size);

                if (debug) {
                    printf("Received: #%d <%s> (%d bytes)\n", message->sequence_number, message->message, message->message_length);
                }

                int split_size = 0;
                char **split_buffer = split(message->message, " ", &split_size);

                if (split_size == 2 && strcmp(split_buffer[0], "ACK") == 0) {
                    // Received an ACK
                    int ack_sequence_number = atoi(split_buffer[1]);

                    if (ack_sequence_number == sequence_number) {
                        message_received = 1;
                    }
                } else {
                    if (debug) {
                        printf("Message received was not an ACK\n");
                    }
                }

                for (int i = 0; i < split_size; i++) {
                    free(split_buffer[i]);
                }
                free(split_buffer);

                free(message->message);
                free(message);
            }

            gettimeofday(&tv, 0);
            current_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        } while (current_time - sent_time < ACK_TIMEOUT && !message_received);

        if (!message_received) {
            resend_attempt++;
        }
    } while (!message_received && resend_attempt < MAX_RESEND_ATTEMPTS);

    if (resend_attempt >= MAX_RESEND_ATTEMPTS) {
        // Max number of resends exceeded
        if (debug) {
            printf("Max number of resends exceeded. Giving up.\n");
        }

        free(package);

        return -1;
    }

    sequence_number = (sequence_number + 1) % 2;

    free(package);

    return size;
}

/**
 * Receives a message from someone.
 * @param sock          Socket to use
 * @param buffer        Buffer to send
 * @param len           Length of buffer
 * @param flags         Flags
 * @param src_addr      Source Address
 * @param addr_len      Size of src_addr
 *
 * @return Received data size
 */
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

        *addr_len = sizeof(*src_addr);
    }

    // Get the message
    int size = recvfrom(sock, buffer, len, flags, src_addr, (socklen_t *) addr_len);

    MESSAGE *message = message_decode(buffer, size);

    size = message->message_length;

    if (debug) {
        printf("Received: #%d <%s> (%d bytes)\n", message->sequence_number, message->message, message->message_length);
    }

    // Respond with an ACK for the sequence number
    if ((arq_ack(sock, message->sequence_number, src_addr, *addr_len)) <= 0) {
        fprintf(stderr, "Could not send ACK.\n");
    }

    memset(buffer, 0, len);
    memcpy(buffer, message->message, message->message_length);

    // Free anything alloc'd
    if (src_addr_malloc) {
        free(src_addr);

        src_addr = 0;
    }

    if (addr_len_malloc) {
        free(addr_len);

        addr_len = 0;
    }

    free(message->message);
    free(message);
    
    // See if we've received a MPS message
    int split_size = 0;
    char **split_buffer = split(buffer, " ", &split_size);
    
    if (split_size == 2 && strcmp(split_buffer[0], "MPS") == 0) {
        // Update the MPS
        max_packet_size = atoi(split_buffer[1]);

        if (debug) {
            printf("Updated max packet size to %d byte(s)\n", max_packet_size);
        }

        for (int i = 0; i < split_size; i++) {
            free(split_buffer[i]);
        }
        free(split_buffer);

        // Seamless ARQ Recvfrom
        return arq_recvfrom(sock, buffer, len, flags, src_addr, addr_len);
    }

    for (int i = 0; i < split_size; i++) {
        free(split_buffer[i]);
    }
    free(split_buffer);
    
    return size;
}

/**
 * Sends an ACK
 * @param sock                  Socket to use
 * @param sequence_number_ack   Sequence number to ACK
 * @param dest_addr             Destination Address
 * @param addr_len              Size of dest_addr
 *
 * @return Received data size
 */
ssize_t arq_ack(int sock, int sequence_number_ack, struct sockaddr *dest_addr, int addr_len) {
    char *message = calloc(arq_get_max_packet_size(), sizeof(char));
    sprintf(message, "ACK %d", sequence_number_ack);

    char *package = calloc(arq_get_max_packet_size(), sizeof(char));
    int package_size = message_encode(package, arq_get_max_packet_size(), sequence_number, message, strlen(message));

    int size = sendto_dropper(sock, package, package_size, 0, dest_addr, addr_len);

    if (debug) {
        printf("Sending: #%d <%s> (%d bytes)\n", sequence_number, message, strlen(message));

        if (size <= 0) {
            printf("Error: %s\n", strerror(errno));
        }
    }

    free(message);
    free(package);

    return size;
}

/**
 * Get the max data size allowed in a packet.
 *
 * @return Max data size
 */
int arq_get_max_data_size() {
    return max_packet_size - PACKET_META_SIZE;
}

/**
 * Get the max packet size allowed.
 *
 * @return Max packet size
 */
int arq_get_max_packet_size() {
    if (max_packet_size <= 0) {
        return 255;
    }

    return max_packet_size;
}
