#include "arq.h"

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
    void *seq_buffer = malloc(sizeof(char) * BUFFER_MAX_SIZE);

    sprintf(seq_buffer, "%d ", sequence_number);
    strncat(seq_buffer, buffer, BUFFER_MAX_SIZE - strlen(seq_buffer));

    if (debug) {
        printf("Sending: %s\n", (char *) seq_buffer);
    }

    int size = sendto_dropper(sock, seq_buffer, len, flags, dest_addr, addr_len);

    char recv_buffer[BUFFER_MAX_SIZE];
    int recv_buffer_size;
    
    memset(recv_buffer, 0, BUFFER_MAX_SIZE);

    if ((recv_buffer_size = recvfrom(sock, recv_buffer, BUFFER_MAX_SIZE, 0, 0, 0)) < 0) {
        printf("Recvd Buffer Size %d\n", recv_buffer_size);
        fprintf(stderr, "Could not receive message from server.");
        exit(2);
    }

    printf("Received a message from the server!\n");

    int split_size = 0;
    char **split_buffer = split(recv_buffer, " ", &split_size);

    if (split_size == 2 && strcmp(split_buffer[0], "ACK") == 0) {
        printf("ACK received - %d\n", atoi(split_buffer[1]));
    }

    sequence_number = (sequence_number + 1) % 2;

    return size;
}

ssize_t arq_recvfrom(int sock, void *buffer, size_t len, int flags, struct sockaddr *src_addr, int *addr_len) {
    int size = recvfrom(sock, buffer, len, flags, src_addr, (socklen_t *) addr_len);

    int split_size = 0;
    char **split_buffer = split(buffer, " ", &split_size);

    if (split_size <= 0) {
        fprintf(stderr, "Did not receive a sequence number.\n");
    }

    arq_ack(sock, atoi(split_buffer[0]), src_addr, *addr_len);
    
    return size;
}

ssize_t arq_ack(int sock, int sequence_number, struct sockaddr *dest_addr, int addr_len) {
    char buffer[BUFFER_MAX_SIZE];

    sprintf(buffer, "ACK %d", sequence_number);

    int size = sendto_dropper(sock, buffer, strlen(buffer), 0, dest_addr, addr_len);

    if (debug || 1) {
        printf("Sent an ACK\n");
    }

    return size;
}
