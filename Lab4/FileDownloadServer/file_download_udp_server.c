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

/*
    Protocol Data Units (PDUs) Data Structure
    Types:
        - C -> Filename
        - D -> Data
        - F -> Final
        - E -> Error

    Data array for the PDU
*/
struct pdu {
    char type;
    char data[100];
};

/*
    sd -> Socket Descriptor
    client -> Client Address information
    client_len -> The length of the Client Address information object
*/
void handle_client(int sd, struct sockaddr_in *client, socklen_t client_len) {
    struct pdu spdu; // Variables needed for the file transfer
    char buffer[BUFLEN];
    int n;
    FILE *file;

    // Receive the filename (1st PDU) from the client
    n = recvfrom(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)client, &client_len); // Socket Descriptor / Buffer to store recived data / flags / source address object / source address object size
    if (n < 0) {
        perror("Error receiving filename");
        return;
    }

    spdu.data[n - 1] = '\0';  // Null-terminate the string
    printf("Requested file: %s\n", spdu.data);

    // Open the file, send an error PDU if it can't be opened
    file = fopen(spdu.data, "rb"); // rb -> read bunary
    if (file == NULL) {
        spdu.type = 'E';
        snprintf(spdu.data, sizeof(spdu.data), "File not found or cannot be opened."); // Set the spdu data to the error message
        printf("Error: %s\n", spdu.data); // Log the error to the server console
        sendto(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)client, client_len); // Send the pdu to the client and exit the process
        return;
    }

    // Send the file contents to the client in chunks with PDUs of type D
    spdu.type = 'D';
    while ((n = fread(spdu.data, sizeof(char), sizeof(spdu.data), file)) > 0) { // output data buffer / size of elements to be read (char)/ number of elements to read / file to read 
        sendto(sd, &spdu, n + 1, 0, (struct sockaddr *)client, client_len);
        // Socket Descriptor / data to be sent / number of bytes to send / for specail flags (none slected) / client address information / size of the address information
    }

    // Send final PDU
    spdu.type = 'F';
    sendto(sd, &spdu, 1, 0, (struct sockaddr *)client, client_len);
    fclose(file);
}

int main(int argc, char *argv[]) {
    int sd, port;
    struct sockaddr_in server, client; // address information about the server and client
    socklen_t client_len = sizeof(client); // Gets the size of the client address information to send to the handle_client method

    // Determine the port from command-line arguments or use default
    port = (argc == 2) ? atoi(argv[1]) : DEFAULT_PORT;

    // Create a UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) { // AF_INET -> IPv4 / SOCK_DGRAM -> UDP (User Datagram Protocal) / Protocal
        perror("Can't create socket");
        exit(1);
    }

    // Set up the server address structure
    bzero((char *)&server, sizeof(server)); // Creates a server address structure initialized to 0
    server.sin_family = AF_INET; // IPv4
    server.sin_port = htons(port); // Converts the port number to a network byte
    server.sin_addr.s_addr = htonl(INADDR_ANY); // Allows the server to accept connections on any of its IP addresses

    // Bind the socket to the address
    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) { // socket descriptor / server address information / size of server address information
        perror("Can't bind socket"); // on bind fail
        exit(1);
    }

    printf("Server listening on port %d\n", port); // Log port used for debugging

    // Main loop to handle client requests
    while (1) {
        handle_client(sd, &client, client_len);
    }

    close(sd);
    return 0;
}
