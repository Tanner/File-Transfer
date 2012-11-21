#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "utilities.h"
#include "arq.h"

#define MAX_PENDING 5

void send_error(int sock, struct sockaddr *dest_addr, int addr_len);

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;
	int client_address_size;
	unsigned short server_port;
    int loss_percentage;

	if (argc < 3 || argc > 4){
		fprintf(stderr, "Usage:  %s <Server Port> <Loss Percentage> [-d]\n", argv[0]);
		exit(1);
	}

	server_port = atoi(argv[1]);
    loss_percentage = atoi(argv[2]);

    if (argc == 4 && strcmp(argv[3], "-d") == 0) {
        printf("Debug mode on.\n");
        debug = 1;
    }

    // Set up ARQ with loss_percentage and max packet size
    if (arq_init(loss_percentage, 0) < 0) {
        fprintf(stderr, "Unable to set up ARQ\n");
        fprintf(stderr, "Invalid loss percentage - Must be between 0 and 100.\n");
        exit(2);
    }
	// Create socket for incoming connections
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "Could not create socket.");
		exit(2);
	}

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(server_port);

	// Bind to the server address
	if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
		fprintf(stderr, "Could not bind to server address.");
		exit(2);
	}

	printf("Server started...\n");

    int message_size;
    char *buffer = malloc(sizeof(char) * BUFFER_MAX_SIZE);

	while(1) {
		client_address_size = sizeof(client_address);

        if ((message_size = arq_recvfrom(sock, buffer, BUFFER_MAX_SIZE, 0, (struct sockaddr *) &client_address, &client_address_size)) < 0) {
            fprintf(stderr, "Could not receive message from client.");
            exit(2);
        }

        int split_size = 0;
        char **split_buffer = split(buffer, " ", &split_size);

        char *client = inet_ntoa(client_address.sin_addr);

        if (split_size > 0) {
            // Handle REQUEST
            if (strcmp(split_buffer[0], "REQUEST") == 0) {
                if (split_size >= 2) {
                    // Got a request with a file name
                    char *file = split_buffer[1];

                    FILE *fp = fopen(file, "r");
                    void *chunk = calloc(6, sizeof(char));

                    printf("%s - Received request for %s\n", client, file);

                    if (!fp) {
                        // Oh no the file error
                        printf("%s - Could not open file %s\n", client, file);

                        send_error(sock, (struct sockaddr *) &client_address, client_address_size);
                    } else {
                        // File was opened successfully
                        printf("%s - Starting transfer\n", client);

                        while (fread(chunk, 5, 1, fp) > 0) {
                            memset(buffer, 0, sizeof(char) * BUFFER_MAX_SIZE);
                            sprintf(buffer, "SEND %s", (char *) chunk);

                            arq_sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) &client_address, client_address_size);
                        }

                        if (feof(fp) != 0) {
                            // End of file was reached
                            printf("%s - Transfer complete\n", client); 
                            printf("%s - Terminating connection\n", client); 

                            memset(buffer, 0, BUFFER_MAX_SIZE);
                            strcpy(buffer, "EOF");

                            arq_sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) &client_address, client_address_size);

                            printf("%s - Connection terminated\n", client);
                        } else if (ferror(fp) != 0) {
                            // An error occurred
                            printf("%s - Error occurred while reading file %s\n", client, file);
                            printf("%s - Terminating connection\n", client); 

                            send_error(sock, (struct sockaddr *) &client_address, client_address_size);

                            printf("%s - Connection terminated\n", client);
                        }
                    }
                } else {
                    printf("%s - Incorrect arguments for REQUEST.\n", client);

                    send_error(sock, (struct sockaddr *) &client_address, client_address_size);
                }
            } else {
                printf("%s - Unknown command.\n", client);
            }
        } else {
            printf("%s - Unsure what to do.\n", client);
        }
    }

    free(buffer);
}

void send_error(int sock, struct sockaddr *dest_addr, int addr_len) {
    char *buffer = "ERROR";

    if (arq_sendto(sock, buffer, strlen(buffer), 0, dest_addr, addr_len) == -1) {
        fprintf(stderr, "Could not send error to client.\n");
        exit(2);
    }
}
