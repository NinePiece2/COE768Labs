#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "constants.h"
#include <limits.h> 

// Structure to store file information
// filename: Name of the file to be shared
// ip: IP address of the machine hosting the file
// port: Port number where the file can be accessed
typedef struct {
    char filename[FILENAME_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port;
    int timeUsed;
    char peerName[PEER_NAME_SIZE];
} FileEntry;

typedef struct {
    char filename[FILENAME_SIZE];
} FoundEntry;

// File registry to store registered files
FileEntry file_registry[MAX_ENTRIES];  // Array to store registered files
int entry_count = 0;                   // Current count of registered entries

// Adds a new file entry to the registry
// Parameters:
// - filename: The name of the file to register
// - ip: The IP address of the machine hosting the file
// - port: The port number for access
// - peerName: The name of the peer hosting the file
int add_file_entry(const char *filename, const char *ip, int port, char *peerName) {
    if (entry_count >= MAX_ENTRIES) {
        printf("Registry full, cannot register more files.\n");
        return -1;
    }
    strncpy(file_registry[entry_count].filename, filename, FILENAME_SIZE);
    strncpy(file_registry[entry_count].ip, ip, INET_ADDRSTRLEN);
    strncpy(file_registry[entry_count].peerName, peerName, PEER_NAME_SIZE);
    file_registry[entry_count].port = port;
    file_registry[entry_count].timeUsed = 0;
    entry_count++;
    printf("Registered file: %s at %s:%d\n", filename, ip, port);
    return 0;
}

// Removes a file entry from the registry
// Parameters:
// - filename: The name of the file to deregister
// - ip: The IP address associated with the file
// - port: The port number associated with the file
int remove_file_entry(const char *filename, const char *ip, int port) {
    int i, j;
    for (i = 0; i < entry_count; i++) {
        if (strcmp(file_registry[i].filename, filename) == 0 &&
            strcmp(file_registry[i].ip, ip) == 0 &&
            file_registry[i].port == port) {
            // Shift remaining entries to fill the gap
            for (j = i; j < entry_count - 1; j++) {
                file_registry[j] = file_registry[j + 1];
            }
            entry_count--;
            printf("Deregistered file: %s from IP: %s and port: %d\n", filename, ip, port);
            return 0;
        }
    }
    printf("File not found in registry: %s at IP: %s and port: %d\n", filename, ip, port);
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
            char peerName[PEER_NAME_SIZE];
            if (sscanf(request.data, "%10s %10s %d", peerName, filename, &tcp_port) == 3) {
                // Check if the same peer name and file already exists
                int conflict = 0;
                int i;
                for (i = 0; i < entry_count; i++) {
                    if (strcmp(file_registry[i].filename, filename) == 0 && strcmp(file_registry[i].peerName, peerName) == 0) {
                        conflict = 1;
                        break;
                    }
                }

                if (conflict) {
                    response.type = ERROR;
                    snprintf(response.data, sizeof(response.data), "Peer name conflict, choose another name.");
                } else {
                    // Add file to registry
                    if (add_file_entry(filename, client_ip, tcp_port, peerName) == 0) {
                        response.type = ACKNOWLEDGE;
                        snprintf(response.data, sizeof(response.data), "Registration successful.");
                    } else {
                        response.type = ERROR;
                        snprintf(response.data, sizeof(response.data), "Registration failed.");
                    }
                }
            } else {
                response.type = ERROR;
                snprintf(response.data, sizeof(response.data), "Invalid registration format.");
            }
            // Send response to the client
            sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);

        // Handle DEREGISTER request
        } else if (request.type == DEREGISTER) {
            printf("Deregister request for content: %s\n", request.data);

            // Parse filename, IP, and port from the request data
            char filename[FILENAME_SIZE];
            int client_port;
            if (sscanf(request.data, "%99[^:]:%d", filename, &client_port) == 2) {
                if (remove_file_entry(filename, client_ip, client_port) == 0) {
                    response.type = ACKNOWLEDGE;
                    strcpy(response.data, "Deregistration successful.");
                } else {
                    response.type = ERROR;
                    strcpy(response.data, "Deregistration failed.");
                }
            } else {
                response.type = ERROR;
                strcpy(response.data, "Invalid deregistration format.");
            }
            // Send response to the client
            sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);

        // Handle LIST_CONTENT request to find each file's least used server
        } else if (request.type == LIST_CONTENT) {
            printf("List request for content\n");
            response.type = LIST_CONTENT;
            response.data[0] = '\0';  // Initialize response data as an empty string

            int found = 0;
            FoundEntry files_found[MAX_ENTRIES];
            int i, j, k;

            for (j = 0; j < entry_count; j++) {
                int exists = 0;
                for (k = 0; k < found; k++) {
                    if (strcmp(file_registry[j].filename, files_found[k].filename) == 0) {
                        exists = 1;
                        break;
                    }
                }

                if (!exists) {
                    // Find the least used entry for the current filename
                    int min_time_used = INT_MAX;
                    int min_index = -1;

                    for (i = 0; i < entry_count; i++) {
                        if (strcmp(file_registry[i].filename, file_registry[j].filename) == 0 &&
                            file_registry[i].timeUsed < min_time_used) {
                            min_time_used = file_registry[i].timeUsed;
                            min_index = i;
                        }
                    }

                    // Add the least used entry to the response
                    if (min_index != -1) {
                        strcpy(files_found[found].filename, file_registry[min_index].filename);
                        found++;

                        file_registry[min_index].timeUsed++;
                        char entry_info[FILENAME_SIZE + PEER_NAME_SIZE + INET_ADDRSTRLEN + 10];
                        snprintf(entry_info, sizeof(entry_info), "%s:%s:%s:%d", file_registry[min_index].peerName, file_registry[min_index].filename, file_registry[min_index].ip, file_registry[min_index].port);
                        strncat(response.data, entry_info, sizeof(response.data) - strlen(response.data) - 1);
                        strncat(response.data, ", ", sizeof(response.data) - strlen(response.data) - 1);
                    }
                }
            }

            // Remove the trailing comma and space
            if (strlen(response.data) > 2) {
                response.data[strlen(response.data) - 2] = '\0';
            }

            if (found) {
                // Send response with all entries found
                sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);
            } else {
                response.type = ERROR;
                strcpy(response.data, "File(s) not found, no data registered.");
                // Send response to the client
                sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);
            }
        }
        // Handle SEARCH request
        else if (request.type == SEARCH) {
            // Search for a file
            printf("Search request for content\n");
            response.type = SEARCH;
            response.data[0] = '\0';  // Initialize response data as an empty string

            char peer_name[PEER_NAME_SIZE] = {0};  // 10 bytes + null terminator
            char filename[FILENAME_SIZE] = {0};
            sscanf(request.data, "%10s %10s", peer_name, filename);
            printf("Requested data: %s\n", request.data);
            printf("Peer name: %s, Filename: %s\n", peer_name, filename);
            int found = 0;
            int i;
            for (i = 0; i < entry_count; i++) {
                if (strcmp(file_registry[i].filename, filename) == 0) {
                    // Check if the peer name matches
                    if (strcmp(file_registry[i].peerName, peer_name) == 0) {
                        char entry_info[INET_ADDRSTRLEN + 10];  // Buffer for IP and port
                        snprintf(entry_info, sizeof(entry_info), "%s:%d", file_registry[i].ip, file_registry[i].port);
                        
                        strncat(response.data, entry_info, sizeof(response.data) - strlen(response.data) - 1);
                        found = 1;
                    }
                }
            }

            if (found) {
                // Send response with the found entry
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