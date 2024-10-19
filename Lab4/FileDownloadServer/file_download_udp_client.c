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

struct pdu {
    char type;
    char data[100];
};

int main(int argc, char *argv[]) {
    int sd, port;
    struct sockaddr_in server;
    struct hostent *hp;
    socklen_t server_len = sizeof(server);
    struct pdu spdu;
    FILE *file;
    int n;

    // Check command-line arguments
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <server_ip> [port]\n", argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    port = (argc == 3) ? atoi(argv[2]) : DEFAULT_PORT;

    // Create UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Can't create socket");
        exit(1);
    }

    // Set up the server address structure
    bzero((char *)&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address\n");
        exit(1);
    }

    // Get the filename from the user
    printf("Enter the filename to request from the server: ");
    fgets(spdu.data, sizeof(spdu.data), stdin);
    spdu.data[strlen(spdu.data) - 1] = '\0';  // Remove newline
    spdu.type = 'C';

    // Send filename PDU to the server
    sendto(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)&server, server_len);

    // Open local file to save data
    file = fopen(spdu.data, "wb");
    if (file == NULL) {
        perror("Error opening file");
        close(sd);
        exit(1);
    }

    // Receive file data
    while ((n = recvfrom(sd, &spdu, sizeof(spdu), 0, (struct sockaddr *)&server, &server_len)) > 0) {
        if (spdu.type == 'E') {
            printf("Error: %s\n", spdu.data);
            remove(spdu.data); // Delete file if error
            break;
        } else if (spdu.type == 'D') {
            fwrite(spdu.data, sizeof(char), n - 1, file);
        } else if (spdu.type == 'F') {
            printf("File transfer complete.\n");
            break;
        }
    }

    fclose(file);
    close(sd);
    return 0;
}
