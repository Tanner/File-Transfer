#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "utilities.h"
#include "dropper.h"

#ifndef __ARQ_H__
#define __ARQ_H__

#define MIN_PACKET_SIZE 5   // Length of 'ACK #'

int debug;

typedef struct _EXPECT {
    ssize_t size;
    int messages_remaining;
} EXPECT;

int arq_init(int loss_percentage);

ssize_t arq_sendto(int sock, void *buffer, size_t len, int flags, struct sockaddr *dest_addr, int addr_len);

ssize_t arq_recvfrom(int sock, char *buffer, size_t len, int flags, struct sockaddr *src_addr, int *addr_len);
EXPECT * arq_recvfrom_expect(int sock, char *buffer, size_t len, int flags, struct sockaddr *src_addr, int *addr_len, int expect_handled);

ssize_t arq_ack(int sock, int sequence_number, struct sockaddr *dest_addr, int addr_len);

#endif
