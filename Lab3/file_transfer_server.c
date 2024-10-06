#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_TCP_PORT 3000    /* Default port */
#define BUFLEN 256              /* Buffer length */

void handle_client(int new_sd) {
    char filename[BUFLEN];
    char buffer[BUFLEN];
    int n;

    // Receive the filename from the client
    n = read(new_sd, filename, BUFLEN - 1);
    if (n < 0) {
        fprintf(stderr, "Error reading filename from socket.\n");
        return;
    }
    filename[n] = '\0'; // Null-terminate the string

    printf("Requested file: %s\n", filename);

    // Try to open the file
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        // If the file can't be opened, send an error message
        buffer[0] = 'E'; // Error indicator
        strcpy(buffer + 1, "Error: File not found or cannot be opened.\n");
        write(new_sd, buffer, strlen(buffer) + 1);
        close(new_sd);
        return;
    }

    // Send the file contents to the client
    buffer[0] = 'F'; // File indicator
    while ((n = fread(buffer + 1, sizeof(char), BUFLEN - 1, file)) > 0) {
        write(new_sd, buffer, n + 1); // Send the packet with data
    }

    fclose(file);
    close(new_sd); // Close the connection after the transfer is complete
}

int main(int argc, char *argv[]) {
    int sd, new_sd, client_len, port;
    struct sockaddr_in server, client;

    port = (argc == 2) ? atoi(argv[1]) : SERVER_TCP_PORT;

    /* Create a TCP stream socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }

    /* Set up the server address structure */
    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the socket */
    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        fprintf(stderr, "Can't bind name to socket\n");
        exit(1);
    }

    /* Listen for connections */
    listen(sd, 5);

    printf("Server listening on port %d\n", port);

    while (1) {
        client_len = sizeof(client);
        new_sd = accept(sd, (struct sockaddr *)&client, &client_len);
        if (new_sd < 0) {
            fprintf(stderr, "Can't accept client connection\n");
            exit(1);
        }
        handle_client(new_sd);
    }

    close(sd);
    return 0;
}
