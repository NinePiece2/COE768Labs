#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_TCP_PORT 3000    /* Default port */
#define BUFLEN 256              /* Buffer length */

int main(int argc, char *argv[]) {
    int sd, port;
    struct hostent *hp;
    struct sockaddr_in server;
    char buffer[BUFLEN], filename[BUFLEN];
    int n;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <server_host> [port]\n", argv[0]);
        exit(1);
    }

    char *host = argv[1];
    port = (argc == 3) ? atoi(argv[2]) : SERVER_TCP_PORT;

    /* Create a TCP stream socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }

    /* Set up the server address structure */
    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    /* Get the host address */
    if ((hp = gethostbyname(host)) == NULL) {
        fprintf(stderr, "Can't get server's address\n");
        exit(1);
    }
    bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);

    /* Connect to the server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        fprintf(stderr, "Can't connect to server\n");
        exit(1);
    }

    /* Get the filename from the user */
    printf("Enter the filename to request from the server: ");
    fgets(filename, BUFLEN, stdin);
    filename[strlen(filename) - 1] = '\0';  // Remove the newline character

    /* Send the filename to the server */
    write(sd, filename, strlen(filename));

    /* Receive the file or error message from the server */
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Error opening file to write received data.\n");
        close(sd);
        exit(1);
    }

    while ((n = read(sd, buffer, BUFLEN)) > 0) {
        if (buffer[0] == 'E') {
            // Handle error message
            printf("Error from server: %s\n", buffer + 1);
			remove(filename); // Delete the created file if there is an error from the server
            break;
        } else if (buffer[0] == 'F') {
            // Write file data to the file
            fwrite(buffer + 1, sizeof(char), n - 1, file);
        }
    }

    fclose(file);
    close(sd);

    if (n == 0) {
        printf("File transfer complete.\n");
    } else if (n < 0) {
        printf("Error reading from server.\n");
    }

    return 0;
}
