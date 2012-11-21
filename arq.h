#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "utilities.h"
#include "dropper.h"
#include "message.h"

#ifndef __ARQ_H__
#define __ARQ_H__

#define MIN_PACKET_SIZE 10

int debug;

typedef struct _EXPECT {
    ssize_t size;
    int messages_remaining;
} EXPECT;

int arq_init(int loss_percentage, int max_packet_size);

ssize_t arq_inform_send(int sock, struct sockaddr *dest_addr, int addr_len);

ssize_t arq_sendto(int sock, void *buffer, size_t len, int flags, struct sockaddr *dest_addr, int addr_len);

ssize_t arq_recvfrom(int sock, char *buffer, size_t len, int flags, struct sockaddr *src_addr, int *addr_len);

ssize_t arq_ack(int sock, int sequence_number, struct sockaddr *dest_addr, int addr_len);

int arq_get_max_data_size();

#endif
