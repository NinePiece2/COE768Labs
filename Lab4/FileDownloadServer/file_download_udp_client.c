#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

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

int main(int argc, char *argv[]) {
    int sd, port; // socket descriptor and port
    struct sockaddr_in server; // server address information
    struct hostent *hp;
    socklen_t server_len = sizeof(server);
    struct pdu spdu; // PDU instance to send and recieve data
    FILE *file; // File object for sotring the downloaded file
    int n;
    char filename[BUFLEN];

    // Check command-line arguments
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <server_ip> [port]\n", argv[0]);
        exit(1);
    }

    // Decodes the server IP and port
    char *server_ip = argv[1];
    port = (argc == 3) ? atoi(argv[2]) : DEFAULT_PORT;

    // Create UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) { // AF_INET -> IPv4 / SOCK_DGRAM -> UDP (User Datagram Protocal) / Protocal
        perror("Can't create socket");
        exit(1);
    }

    // Set up the server address structure
    bzero((char *)&server, sizeof(server)); // Creates a server address structure initialized to 0
    server.sin_family = AF_INET; // IPv4
    server.sin_port = htons(port);  // Converts the port number to a network byte
    if (inet_pton(AF_INET, server_ip, &server.sin_addr) <= 0) { // Converts the given server IP Address from text to binary AF_INET -> IPv4 / IP in text / Binary destination
        fprintf(stderr, "Invalid server IP address\n");
        exit(1);
    }

    // Get the filename from the user
    printf("Enter the filename to request from the server: ");
    fgets(spdu.data, sizeof(spdu.data), stdin);
    spdu.data[strlen(spdu.data) - 1] = '\0';  // Remove newline
    spdu.type = 'C';
    strncpy(filename, spdu.data, sizeof(filename) - 1);

    // Send filename PDU to the server
    sendto(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)&server, server_len);

    // Open local file to save data
    file = fopen(spdu.data, "wb"); // wb -> write binary
    if (file == NULL) {
        perror("Error opening file");
        close(sd);
        exit(1);
    }

    // Receive file data
    // Socket Descriptor / Buffer to store recived data / flags / source address object / source address object size
    while ((n = recvfrom(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)&server, &server_len)) > 0) { 
        if (spdu.type == 'E') {
            remove(filename); // Delete the newly created file if there is an error
            printf("Error: %s\n", spdu.data);
            break;
        } else if (spdu.type == 'D') {
            fwrite(spdu.data, sizeof(char), n - 1, file); // Writes the recieved data to the file
        } else if (spdu.type == 'F') {
            printf("File transfer complete.\n"); // Stops the program once done
            break;
        }
    }

    fclose(file);
    close(sd);
    return 0;
}
