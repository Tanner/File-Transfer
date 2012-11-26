#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

typedef struct MESSAGE_ {
    int sequence_number;

    char *message;
    int message_length;
} MESSAGE;

int message_encode(char *buffer, int size, int sequence_number, char *message, int length);
MESSAGE * message_decode(char *buffer, int size);

#endif
