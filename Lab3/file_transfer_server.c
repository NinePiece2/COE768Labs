#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_TCP_PORT 15000    /* Default server port */
#define BUFLEN 256              /* Buffer length */

/* Process client requests */
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

    printf("Requested file: %s\n", filename); // Prints the filename to the server's console

    // Try to open the file, send an error if it can't
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        // If the file can't be opened, send an error message
        buffer[0] = 'E'; // Error indicator
        strcpy(buffer + 1, "Error: File not found or cannot be opened.\n"); // Copies to the buffer's array of chars
        write(new_sd, buffer, strlen(buffer) + 1);
        close(new_sd);
        return;
    }

    // Send the file contents to the client in chunks 256 bytes a chunk of data
    buffer[0] = 'F'; // File indicator
    while ((n = fread(buffer + 1, sizeof(char), BUFLEN - 1, file)) > 0) {
        write(new_sd, buffer, n + 1); // Send the packet with data
    }

    fclose(file);
    close(new_sd); // Close the connection to the client after the transfer is complete
}

int main(int argc, char *argv[]) {
    int sd, new_sd, client_len, port; // Socket descriptor for the server / sd of the client / length of the client address struct / server listen port
    struct sockaddr_in server, client; // Two structures for the server's address and connected client's information

    port = (argc == 2) ? atoi(argv[1]) : SERVER_TCP_PORT; // If the server port is given by the user use it or else use the defualt

    /* Create a TCP stream socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { // Creates a TCP Socket (SOCK_STREAM) using IPv4 (AF_INET)
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }

    /* Set up the server address structure */
    bzero((char *)&server, sizeof(struct sockaddr_in)); // Creates a server address structure initialized to 0
    server.sin_family = AF_INET; // IPv4
    server.sin_port = htons(port); // Converts the port number to a network byte
    server.sin_addr.s_addr = htonl(INADDR_ANY); // Allows the server to accept connections on any of its IP addresses

    /* Bind the socket */
    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) { // Tries to bind the socket to the server address 
        fprintf(stderr, "Can't bind name to socket\n");
        exit(1);
    }

    /* Listen for connections */
    listen(sd, 5); // 5 is the max queue of pending connections

    printf("Server listening on port %d\n", port); // Logs to the server's console

    /* Infinite loop to accept requests */
    while (1) {
        client_len = sizeof(client); // Value used to indicate the amount of memory allocated for the client's address
        new_sd = accept(sd, (struct sockaddr *)&client, &client_len); // Wait's for an incoming connection and accepts it. Set to the new socket descriptor
        if (new_sd < 0) {
            fprintf(stderr, "Can't accept client connection\n");
            exit(1);
        }
        handle_client(new_sd); // Calls the function with the socket which handles the file transfer
    }

    close(sd); // Closes the socket and ends the program (doesn't hit because of infinate loop)
    return 0;
}
