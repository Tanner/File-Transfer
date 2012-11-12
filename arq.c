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
    return sendto_dropper(sock, buffer, len, flags, dest_addr, addr_len);
}

ssize_t arq_recvfrom(int sock, void *buffer, size_t len, int flags, struct sockaddr *src_addr, int addr_len) {
    return recvfrom(sock, buffer, len, flags, src_addr, (socklen_t *) addr_len);
}
