#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_TCP_PORT 15000    /* Default server port */
#define BUFLEN 256              /* Buffer length */

int main(int argc, char *argv[]) {
    int sd, port; // Socket Descriptor and port
    struct hostent *hp; // Pointer to a hostent used for DNS lookup
    struct sockaddr_in server; // Server address
    char buffer[BUFLEN], filename[BUFLEN]; // Buffer's incoming data from server and filename that is taken from the user
    int n; // Stores the number of bytes read

    /* Checks if the number of arguments given is valid, port is optional defualts to 15000*/
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <server_host> [port]\n", argv[0]);
        exit(1);
    }

    char *host = argv[1]; // gets the host ip as a string
    port = (argc == 3) ? atoi(argv[2]) : SERVER_TCP_PORT; // if the port is given use it or else use the defualt port

    /* Create a TCP stream socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { // Creates a TCP Socket (SOCK_STREAM) using IPv4 (AF_INET)
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }

    /* Set up the server address structure */
    bzero((char *)&server, sizeof(struct sockaddr_in)); // Creates a server address structure initialized to 0
    server.sin_family = AF_INET; // IPv4
    server.sin_port = htons(port); // Converts the port number to a network byte

    /* Get the host address */
    if ((hp = gethostbyname(host)) == NULL) { // Resolves the server's hostname to a IP Address
        fprintf(stderr, "Can't get server's address\n");
        exit(1);
    }
    bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length); // Copies the server's IP Address to the server address structure

    /* Connect to the server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) { // Connects to the server using the socket descriptor and the server address structure
        fprintf(stderr, "Can't connect to server\n");
        exit(1);
    }

    /* Get the filename from the user */
    printf("Enter the filename to request from the server: ");
    fgets(filename, BUFLEN, stdin);
    filename[strlen(filename) - 1] = '\0';  // Remove the newline character from enter

    /* Send the filename to the server over the socket using the descriptor */
    write(sd, filename, strlen(filename));

    /* Tries to create the file locally to store the recived data, on failure closes the socket */
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Error opening file to write received data.\n");
        close(sd);
        exit(1);
    }

    /* Continously reads data from the socket/server */
    while ((n = read(sd, buffer, BUFLEN)) > 0) {
        if (buffer[0] == 'E') { // If the message starts with a E it is an error
            // Handle error message
            printf("Error from server: %s\n", buffer + 1);
			remove(filename); // Delete the created file if there is an error from the server
            break;
        } else if (buffer[0] == 'F') { // If the message starts with F it contains file data and shoud be written to the file
            // Write file data to the file
            fwrite(buffer + 1, sizeof(char), n - 1, file); // Starting address / size of character / number of elements to write / file to write to
        }
    }

    fclose(file); // Closes the file and socket
    close(sd);

    /* Prints the result of the file transfer */
    if (n == 0) {
        printf("File transfer complete.\n");
    } else if (n < 0) {
        printf("Error reading from server.\n");
    }

    return 0;
}
