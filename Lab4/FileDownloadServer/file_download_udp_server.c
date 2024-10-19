#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 15000    /* Default server port */
#define BUFLEN 100            /* Buffer length for file data chunks */

struct pdu {
    char type;
    char data[100];
};

void handle_client(int sd, struct sockaddr_in *client, socklen_t client_len) {
    struct pdu spdu;
    char buffer[BUFLEN];
    int n;
    FILE *file;

    // Receive the filename from the client
    n = recvfrom(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)client, &client_len);
    if (n < 0) {
        perror("Error receiving filename");
        return;
    }

    spdu.data[n - 1] = '\0';  // Null-terminate the string
    printf("Requested file: %s\n", spdu.data);

    // Open the file, send an error PDU if it can't be opened
    file = fopen(spdu.data, "rb");
    if (file == NULL) {
        spdu.type = 'E';
        snprintf(spdu.data, sizeof(spdu.data), "Error: File not found or cannot be opened.");
        sendto(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)client, client_len);
        return;
    }

    // Send the file contents to the client in chunks
    spdu.type = 'D';
    while ((n = fread(spdu.data, sizeof(char), sizeof(spdu.data), file)) > 0) {
        sendto(sd, &spdu, n + 1, 0, (struct sockaddr *)client, client_len);
    }

    // Send final PDU
    spdu.type = 'F';
    sendto(sd, &spdu, 1, 0, (struct sockaddr *)client, client_len);

    fclose(file);
}

int main(int argc, char *argv[]) {
    int sd, port;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(client);

    // Determine the port from command-line arguments or use default
    port = (argc == 2) ? atoi(argv[1]) : DEFAULT_PORT;

    // Create a UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Can't create socket");
        exit(1);
    }

    // Set up the server address structure
    bzero((char *)&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the address
    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Can't bind socket");
        exit(1);
    }

    printf("Server listening on port %d\n", port);

    // Main loop to handle client requests
    while (1) {
        handle_client(sd, &client, client_len);
    }

    close(sd);
    return 0;
}
