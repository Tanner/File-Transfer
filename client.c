#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "utilities.h"
#include "arq.h"

int main(int argc, char *argv[]) {
	char *server_ip, *remote_filename, *local_filename;
	unsigned int server_port, max_packet_size;
    unsigned short loss_percentage;

    if (argc < 7 || argc > 8) {
	   fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Remote File> <Local File> <Max Packet Size> <Loss Percentage> [-d]\n", argv[0]);
	   exit(1);
	}

    server_ip = argv[1];
    server_port = atoi(argv[2]);
    remote_filename = argv[3];
    local_filename = argv[4];
    max_packet_size = atoi(argv[5]);
    loss_percentage = atoi(argv[6]);

    if (argc == 8 && strcmp(argv[7], "-d") == 0) {
        printf("Debug mode on.\n");
        debug = 1;
    }

    // Set up ARQ with loss_percentage
    if (arq_init(loss_percentage) < 0) {
        fprintf(stderr, "Unable to set up ARQ\n");
        fprintf(stderr, "Invalid loss percentage - Must be between 0 and 100.\n");
        exit(2);
    }

    // Check max_packet_size
    if (max_packet_size < MIN_PACKET_SIZE) {
        fprintf(stderr, "Max packet size too small - minimum is %d bytes.\n", MIN_PACKET_SIZE);
        
        exit(2);
    }

    if (debug) {
        printf("Remote Filename: %s\n", remote_filename);
        printf("Local Filename: %s\n", local_filename);
        printf("Max Packet Size: %d byte(s)\n", max_packet_size);
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

    char *buffer = malloc(sizeof(char) * BUFFER_MAX_SIZE);
    struct timeval tv;
    time_t start_time, end_time;

    gettimeofday(&tv, 0);
    start_time = tv.tv_sec;

    if (debug) {
        printf("Requesting file from server.\n");
    }

    // Request the file from the server
    sprintf(buffer, "REQUEST %s", remote_filename);
    arq_sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) &server_address, sizeof(server_address));

    printf("Starting transfer...\n");

    FILE *fp = fopen(local_filename, "w");

    // Receive the file from the server
    do {
        memset(buffer, 0, BUFFER_MAX_SIZE);

        arq_recvfrom(sock, buffer, BUFFER_MAX_SIZE, 0, 0, 0);

        // Error checking
        if (strcmp(buffer, "ERROR") == 0) {
            printf("Server returned an error. Exiting.\n");

            close(sock);
            exit(2);
        }

        char *temp = calloc(5, sizeof(char));
        strncpy(temp, buffer, 4 * sizeof(char));
        
        if (strcmp(temp, "SEND") == 0) {
            // SEND detected; extract data
            char *data = malloc(sizeof(char) * (strlen(buffer) - 4));
            strncpy(data, buffer + 5, strlen(buffer) - 4);

            if (fwrite(data, strlen(data), 1, fp) != 1) {
                printf("Error writing to file.\n");
                printf("Terminating connection\n");

                strcpy(buffer, "BYE");
                arq_sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) &server_address, sizeof(server_address));

                printf("Connection terminated\n");

                close(sock);
                exit(2);
            }

            // Ready to continue
            strcpy(buffer, "READY");
            arq_sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) &server_address, sizeof(server_address));
        } else if (strcmp(buffer, "EOF") != 0) {
            if (debug) {
                printf("Unkown response type.\n");
                printf("Received: %s\n", buffer);
            }
        }
    } while (strcmp(buffer, "EOF") != 0);

    printf("Transfer complete.\n");

    gettimeofday(&tv, 0);
    end_time = tv.tv_sec;

    printf("Transfer took %d second(s)\n", (int) (end_time - start_time));

    close(sock);

	exit(0);
}
