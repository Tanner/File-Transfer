#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

typedef struct MESSAGE_ {
    int sequence_number;

    char *message;
} MESSAGE;

int message_encode(char *buffer, int size, int sequence_number, char *message);
MESSAGE * message_decode(char *buffer);

#endif
