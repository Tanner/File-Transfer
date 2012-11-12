#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "utilities.h"
#include "dropper.h"

#ifndef __ARQ_H__
#define __ARQ_H__

int debug;

int arq_init(int loss_percentage);

ssize_t arq_sendto(int sock, void *buffer, size_t len, int flags, struct sockaddr *dest_addr, int addr_len);
ssize_t arq_recvfrom(int sock, void *buffer, size_t len, int flags, struct sockaddr *src_addr, int addr_len);

#endif
