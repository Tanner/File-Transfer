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

int send_error(int sock, struct sockaddr *dest_addr, int addr_len);

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

	while(1) {
		client_address_size = sizeof(client_address);

        char *buffer = calloc(1, sizeof(char) * arq_get_max_packet_size());

        if (arq_recvfrom(sock, buffer, arq_get_max_packet_size(), 0, (struct sockaddr *) &client_address, &client_address_size) < 0) {
            continue;
        }

        char *client = inet_ntoa(client_address.sin_addr);

        int split_size = 0;
        char **split_buffer = split(buffer, " ", &split_size);

        free(buffer);

        if (split_size > 0) {
            // Handle REQUEST
            if (strcmp(split_buffer[0], "REQUEST") == 0) {
                if (split_size >= 2) {
                    // Got a request with a file name
                    char *file = split_buffer[1];

                    printf("%s - Received request for %s\n", client, file);

                    FILE *fp = fopen(file, "r");

                    if (!fp) {
                        // Oh no the file error
                        printf("%s - Could not open file %s\n", client, file);

                        if (send_error(sock, (struct sockaddr *) &client_address, client_address_size) < 0) {
                            fprintf(stderr, "%s - Unable to contact client.\n", client);
                        }
                    } else {
                        // File was opened successfully
                        printf("%s - Starting transfer\n", client);

                        // 5 b/c number of bytes for "SEND "
                        const int command_size = 5;
                        int chunk_size = arq_get_max_data_size() - command_size;

                        int read_chunk_size = 0;

                        do {
                            void *chunk = calloc((size_t) chunk_size, sizeof(char));
                            read_chunk_size = fread(chunk, 1, chunk_size, fp);

                            if (read_chunk_size > 0) {
                                buffer = calloc(arq_get_max_packet_size(), sizeof(char));

                                sprintf(buffer, "SEND %s", (char *) chunk);

                                int size = arq_sendto(sock, buffer, arq_get_max_data_size(), 0, (struct sockaddr *) &client_address, client_address_size);
                                
                                if (size < 0) {
                                    fprintf(stderr, "%s - Unable to contact client.\n", client);

                                    break;
                                }

                                free(buffer);
                            }

                            free(chunk);
                        } while (read_chunk_size > 0);

                        if (feof(fp) != 0) {
                            // End of file was reached
                            printf("%s - Transfer complete\n", client); 
                            printf("%s - Terminating connection\n", client); 

                            buffer = calloc(arq_get_max_packet_size(), sizeof(char));
                            strcpy(buffer, "EOF");

                            if (arq_sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) &client_address, client_address_size) < 0) {
                                fprintf(stderr, "%s - Unable to contact client.\n", client);
                            }

                            printf("%s - Connection terminated\n", client);
                        } else if (ferror(fp) != 0) {
                            // An error occurred
                            printf("%s - Error occurred while reading file %s\n", client, file);
                            printf("%s - Terminating connection\n", client); 

                            if (send_error(sock, (struct sockaddr *) &client_address, client_address_size) < 0) {
                                fprintf(stderr, "%s - Unable to contact client.\n", client);
                            }

                            printf("%s - Connection terminated\n", client);
                        }

                        fclose(fp);
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

        for (int i = 0; i < split_size; i++) {
            free(split_buffer[i]);
        }
        free(split_buffer);

        free(buffer);
    }
}

int send_error(int sock, struct sockaddr *dest_addr, int addr_len) {
    char *buffer = "ERROR";

    return arq_sendto(sock, buffer, strlen(buffer), 0, dest_addr, addr_len);
}
