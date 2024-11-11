#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "constants.h"

typedef struct {
    char filename[FILENAME_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port;
} FileEntry;

FileEntry file_registry[MAX_ENTRIES];  // Array to store registered files
int entry_count = 0;                   // Current count of registered entries

// Add a file entry
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

// Remove a file entry
int remove_file_entry(const char *filename, const char *ip) {
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(file_registry[i].filename, filename) == 0 && strcmp(file_registry[i].ip, ip) == 0) {
            // Shift remaining entries to fill the gap
            for (int j = i; j < entry_count - 1; j++) {
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

// Find a file entry
FileEntry *find_file_entry(const char *filename) {
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(file_registry[i].filename, filename) == 0) {
            return &file_registry[i];
        }
    }
    return NULL;
}

// Main server function for handling UDP communication
void index_server_udp(int server_port) {
    int sd;
    struct sockaddr_in server, client;
    struct pdu request, response;
    socklen_t client_len;
    char client_ip[INET_ADDRSTRLEN];

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Cannot create socket");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);  // Use provided server port
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Cannot bind socket");
        close(sd);
        exit(1);
    }

    printf("Index server is listening on port %d\n", server_port);

    while (1) {
        client_len = sizeof(client);
        if (recvfrom(sd, &request, sizeof(request), 0, (struct sockaddr *)&client, &client_len) == -1) {
            perror("Failed to receive message");
            continue;
        }

        // Capture the client's IP address from the received message
        inet_ntop(AF_INET, &client.sin_addr, client_ip, INET_ADDRSTRLEN);

        if (request.type == REGISTER) {
            printf("Register request for content: %s\n", request.data);

            // Separate filename and port from request.data
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
            sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);

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
            sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);

        } else if (request.type == SEARCH) {
            printf("Search request for content: %s\n", request.data);
            FileEntry *entry = find_file_entry(request.data);
            if (entry) {
                response.type = LIST_CONTENT;
                snprintf(response.data, sizeof(response.data), "%s:%d", entry->ip, entry->port);
            } else {
                response.type = ERROR;
                strcpy(response.data, "File not found.");
            }
            sendto(sd, &response, sizeof(response), 0, (struct sockaddr *)&client, client_len);
        }
    }
    close(sd);
}

int main(int argc, char *argv[]) {
    int server_port = SERVER_PORT;  // Default port

    if (argc > 1) {
        server_port = atoi(argv[1]);  // Use specified port if provided
    }

    index_server_udp(server_port);
    return 0;
}
