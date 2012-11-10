#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "dropper.h"

int debug;

int main(int argc, char *argv[]) {
	char *server_ip, *remote_filename, *local_filename;
	unsigned int server_port, max_packet_size;
    unsigned short loss_percentage;

    if (argc != 7) {
	   fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Remote File> <Local File> <Max Packet Size> <Loss Percentage>\n", argv[0]);
	   exit(1);
	}

    server_ip = argv[1];
    server_port = atoi(argv[2]);
    remote_filename = argv[3];
    local_filename = argv[4];
    max_packet_size = atoi(argv[5]);
    loss_percentage = atoi(argv[6]);

    // Set the dropper loss percentage
    if (set_dropper(loss_percentage) < 0) {
        fprintf(stderr, "Invalid loss percentage - Must be between 0 and 100.");
        exit(2);
    }

	// Create a socket using UDP
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "Could not create socket.\n");
		return -1;
	}

	// Construct the server address structure
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(server_ip);
	server_address.sin_port = htons(server_port);

    close(sock);

	exit(0);
}
