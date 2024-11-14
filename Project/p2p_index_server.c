#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "constants.h"

// Structure to store file information
// filename: Name of the file to be shared
// ip: IP address of the machine hosting the file
// port: Port number where the file can be accessed
typedef struct {
    char filename[FILENAME_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port;
} FileEntry;

// File registry to store registered files
FileEntry file_registry[MAX_ENTRIES];  // Array to store registered files
int entry_count = 0;                   // Current count of registered entries

// Adds a new file entry to the registry
// Parameters:
// - filename: The name of the file to register
// - ip: The IP address of the machine hosting the file
// - port: The port number for access
int add_file_entry(const char *filename, const char *ip, int port) {
    if (entry_count >= MAX_ENTRIES) {
        printf("Registry full, cannot register more files.\n");
        return -1;
    }
    strncpy(file_registry[entry_count].filename, filename, FILENAME_SIZE);
    strncpy(file_registry[entry_count].ip, ip, INET_ADDRSTRLEN);
    file_registry[entry_count].port = port;
    entry_count++;
    printf("Registered file: %s at %s:%d\n", filename, ip, port);
    return 0;
}

// Removes a file entry from the registry
// Parameters:
// - filename: The name of the file to deregister
// - ip: The IP address associated with the file
int remove_file_entry(const char *filename, const char *ip) {
    int i, j;
    for (i = 0; i < entry_count; i++) {
        if (strcmp(file_registry[i].filename, filename) == 0 && strcmp(file_registry[i].ip, ip) == 0) {
            // Shift remaining entries to fill the gap
            for (j = i; j < entry_count - 1; j++) {
                file_registry[j] = file_registry[j + 1];
            }
            entry_count--;
            printf("Deregistered file: %s from IP: %s\n", filename, ip);
            return 0;
        }
    }
    printf("File not found in registry: %s at IP: %s\n", filename, ip);
    return -1;
}

// Searches for a file entry in the registry
// Parameters:
// - filename: The name of the file to search
// Returns a pointer to the file entry if found, otherwise NULL
FileEntry *find_file_entry(const char *filename) {
    int i;
    for (i = 0; i < entry_count; i++) {
        if (strcmp(file_registry[i].filename, filename) == 0) {
            return &file_registry[i];
        }
    }
    return NULL;
}

// Main function for handling incoming UDP requests on the index server
// Parameters:
// - server_port: The port number for the server to listen on
void index_server_udp(int server_port) {
    int sd;
    struct sockaddr_in server, client;
    struct pdu request, response;  // Protocol Data Unit to send and receive data
    socklen_t client_len;
    char client_ip[INET_ADDRSTRLEN];

    // Create UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Cannot create socket");
        exit(1);
    }

    // Set up server address structure
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);  // Use provided server port
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the server address
    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Cannot bind socket");
        close(sd);
        exit(1);
    }

    printf("Index server is listening on port %d\n", server_port);

    // Infinite loop to handle incoming requests
    while (1) {
        client_len = sizeof(client);
        // Receive request from the client
        if (recvfrom(sd, &request, sizeof(request), 0, (struct sockaddr *)&client, &client_len) == -1) {
            perror("Failed to receive message");
            continue;
        }

        // Capture the client's IP address
        inet_ntop(AF_INET, &client.sin_addr, client_ip, INET_ADDRSTRLEN);

        // Handle REGISTER request
        if (request.type == REGISTER) {
            printf("Register request for content: %s\n", request.data);

            // Parse filename and port from the request data
            char filename[FILENAME_SIZE];
            int tcp_port;
            if (sscanf(request.data, "%99[^:]:%d", filename, &tcp_port) == 2) {
                if (add_file_entry(filename, client_ip, tcp_port) == 0) {
                    response.type = ACKNOWLEDGE;
                    strcpy(response.data, "Registration successful.");
                } else {
                    response.type = ERROR;
                    strcpy(response.data, "Registration failed.");
                }
            } else {
                response.type = ERROR;
                strcpy(response.data, "Invalid registration format.");
            }
            // Send response to the client
            sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);

        // Handle DEREGISTER request
        } else if (request.type == DEREGISTER) {
            printf("Deregister request for content: %s\n", request.data);

            // Use client IP address in deregistration
            if (remove_file_entry(request.data, client_ip) == 0) {
                response.type = ACKNOWLEDGE;
                strcpy(response.data, "Deregistration successful.");
            } else {
                response.type = ERROR;
                strcpy(response.data, "Deregistration failed.");
            }
            // Send response to the client
            sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);

        // Handle SEARCH request
        } else if (request.type == SEARCH) {
            printf("Search request for content: %s\n", request.data);
            response.type = LIST_CONTENT;
            response.data[0] = '\0';  // Initialize response data as an empty string

            int found = 0;
            int i;
            for (i = 0; i < entry_count; i++) {
                if (strcmp(file_registry[i].filename, request.data) == 0) {
                    char entry_info[INET_ADDRSTRLEN + 10];  // Buffer for IP and port
                    snprintf(entry_info, sizeof(entry_info), "%s:%d", file_registry[i].ip, file_registry[i].port);
                    if (found > 0) {
                        strncat(response.data, ", ", sizeof(response.data) - strlen(response.data) - 1);
                    }
                    strncat(response.data, entry_info, sizeof(response.data) - strlen(response.data) - 1);
                    found = 1;
                }
            }

            if (found) {
                // Send response with all entries found
                sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);
            } else {
                response.type = ERROR;
                strcpy(response.data, "File not found.");
                // Send response to the client
                sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);
            }
        }
    }
    close(sd);
}

// Entry point of the index server
// Parameters:
// - argc: Number of command-line arguments
// - argv: Array of command-line arguments
int main(int argc, char *argv[]) {
    int server_port = SERVER_PORT;  // Default server port

    if (argc > 1) {
        server_port = atoi(argv[1]);  // Use specified port if provided
    }

    // Start the index server to handle UDP requests
    index_server_udp(server_port);
    return 0;
}