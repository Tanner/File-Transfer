#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "utilities.h"
#include "arq.h"

#define MAX_PENDING 5

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

    // Set up ARQ with loss_percentage
    if (arq_init(loss_percentage) < 0) {
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
    char buffer[BUFFER_MAX_SIZE];

	while(1) {
		client_address_size = sizeof(client_address);

        if ((message_size = arq_recvfrom(sock, buffer, BUFFER_MAX_SIZE, 0, (struct sockaddr *) &client_address, &client_address_size)) < 0) {
            fprintf(stderr, "Could not receive message from client.");
            exit(2);
        }

        printf("Handling client %s\n", inet_ntoa(client_address.sin_addr));

        printf("Received: %s\n", buffer);

        //arq_sendto(sock, buffer, message_size, 0, (struct sockaddr *) &client_address, sizeof(client_address));
	}
}
