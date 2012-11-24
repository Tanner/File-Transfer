#include "message.h"

int message_encode(char *buffer, int size, int sequence_number, char *message) {
    memset(buffer, 0, size);

    sprintf(buffer, "%c%s", sequence_number, message);
    
    return strlen(message) + 1;
}

MESSAGE * message_decode(char *buffer) {
    MESSAGE *message = malloc(sizeof(MESSAGE));
    assert(message);

    message->sequence_number = buffer[0];

    message->message = calloc(strlen(buffer + 1) + 1, sizeof(char));
    assert(message->message);

    memmove(message->message, buffer + 1, strlen(buffer + 1));

    return message;
}
