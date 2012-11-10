#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define DEFAULT_SERVER_PORT 7

int main(int argc, char *argv[]) {
	char *server_ip;
	unsigned int server_port;

	if ((argc < 3) || (argc > 3)) {
	   fprintf(stderr, "Usage: %s <Server IP> [<Echo Port>]\n", argv[0]);
	   exit(1);
	}

	server_ip = argv[1]; // First argument – Server IP (dotted quad)

	// Use a given port if one is provided, otherwise the default
	if (argc == 3) {
		server_port = atoi(argv[2]);
	} else {
		server_port = DEFAULT_SERVER_PORT;
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
