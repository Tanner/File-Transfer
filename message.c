#include "message.h"
#include "assert.h"

int message_encode(char *buffer, int size, int sequence_number, char *message, int length) {
    assert(size - 1 >= length);

    memset(buffer, 0, size);

    buffer[0] = sequence_number;

    memcpy(buffer + 1, message, length);
    
    return length + 1;
}

MESSAGE * message_decode(char *buffer, int size) {
    MESSAGE *message = calloc(1, sizeof(MESSAGE));
    assert(message);

    message->sequence_number = buffer[0];

    message->message = calloc(size - 1, sizeof(char));
    assert(message->message);

    memcpy(message->message, buffer + 1, size - 1);

    message->message_length = size - 1;

    return message;
}
