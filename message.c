#include "message.h"
#include "assert.h"

/**
 * Encodes a message into a string (packet).
 *
 * @param buffer            Resulting string
 * @param size              Size of buffer
 * @param sequence_number   Sequence number
 * @param message           Message to be encoded
 * @param length            Length of message
 *
 * @return Length of resulting string
 */
int message_encode(char *buffer, int size, int sequence_number, char *message, int length) {
    assert(size - 3 >= length);

    memset(buffer, 0, size);

    buffer[0] = sequence_number;

    int *msg_length = (int *) &buffer[1];
    *msg_length = length;

    memcpy(buffer + 1 + 32, message, length);
    
    return length + 1 + 32;
}

/**
 * Decodes a message into its separate parts.
 *
 * @param buffer    Buffer to decode
 * @param size      Size of buffer
 *
 * @return Message
 */
MESSAGE * message_decode(char *buffer, int size) {
    MESSAGE *message = calloc(1, sizeof(MESSAGE));
    assert(message);

    message->sequence_number = buffer[0];

    int *msg_length = (int *) &buffer[1];
    message->message_length = *msg_length;

    message->message = calloc(message->message_length, sizeof(char));
    assert(message->message);

    memcpy(message->message, buffer + 1 + 32, message->message_length);

    return message;
}
