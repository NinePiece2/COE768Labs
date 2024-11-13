#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include "constants.h"

// Function prototypes
void deregister_content(const char *server_ip, int server_port, const char *filename);
void *tcp_server_thread(void *args);

// Structure to pass arguments to the TCP server thread
// port: Port to serve the file on
// filename: Name of the file to serve
typedef struct {
    int port;
    char filename[100];
} ServerArgs;

// Structure for file registry to track active files, ports, and thread IDs
// filename: Name of the registered file
// port: Port where the file is being served
// thread_id: ID of the thread serving the file
typedef struct {
    char filename[100];
    int port;
    pthread_t thread_id;
} FileRegistryEntry;

// Array to store active file servers
FileRegistryEntry registry[MAX_ENTRIES];  // Array to store active file servers
int registry_count = 0;                   // Track the number of registered files

// Checks if a file is already registered
// Parameters:
// - filename: The name of the file to check
// Returns 1 if the file is registered, 0 otherwise
int is_file_registered(const char *filename) {
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(registry[i].filename, filename) == 0) {
            return 1;  // File is already registered
        }
    }
    return 0;
}

// Removes an entry from the registry
// Parameters:
// - filename: The name of the file to remove
void remove_registry_entry(const char *filename) {
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(registry[i].filename, filename) == 0) {
            pthread_cancel(registry[i].thread_id);  // Cancel the thread serving the file
            for (int j = i; j < registry_count - 1; j++) {
                registry[j] = registry[j + 1];  // Shift remaining entries
            }
            registry_count--;
            break;
        }
    }
}

// Adds an entry to the registry
// Parameters:
// - filename: The name of the file to register
// - port: The port number to serve the file
// - thread_id: The ID of the thread serving the file
void add_registry_entry(const char *filename, int port, pthread_t thread_id) {
    if (registry_count < MAX_ENTRIES) {
        strncpy(registry[registry_count].filename, filename, sizeof(registry[registry_count].filename) - 1);
        registry[registry_count].port = port;
        registry[registry_count].thread_id = thread_id;
        registry_count++;
    } else {
        printf("Registry is full, cannot add more entries.\n");
    }
}

// Deregisters all files on exit
// Parameters:
// - server_ip: IP address of the index server
// - server_port: Port number of the index server
void cleanup_on_exit(const char *server_ip, int server_port) {
    for (int i = 0; i < registry_count; i++) {
        deregister_content(server_ip, server_port, registry[i].filename);
        pthread_cancel(registry[i].thread_id);
    }
    registry_count = 0;
}

// Generates a random port number between 20000 and 65535
// Returns a random port number
int get_random_port() {
    int min_port = 20000;
    int max_port = 65535;
    srand(time(NULL));
    return min_port + rand() % (max_port - min_port + 1);
}

// Registers content with the index server
// Parameters:
// - server_ip: IP address of the index server
// - server_port: Port number of the index server
// - filename: The name of the file to register
// - tcp_port: The port number to serve the file
void register_content(const char *server_ip, int server_port, const char *filename, int tcp_port) {
    if (is_file_registered(filename)) {
        printf("File '%s' is already registered.\n", filename);
        return;
    }

    int sd;
    struct sockaddr_in server;
    struct pdu request;
    socklen_t server_len = sizeof(server);

    // Create UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Cannot create socket");
        exit(1);
    }

    // Set up server address structure
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server.sin_addr);

    // Prepare the registration request
    request.type = REGISTER;
    snprintf(request.data, sizeof(request.data), "%s:%d", filename, tcp_port);

    // Send the registration request to the index server
    sendto(sd, &request, sizeof(request), 0, (struct sockaddr *)&server, server_len);
    close(sd);
}

// Deregisters content with the index server
// Parameters:
// - server_ip: IP address of the index server
// - server_port: Port number of the index server
// - filename: The name of the file to deregister
void deregister_content(const char *server_ip, int server_port, const char *filename) {
    int sd;
    struct sockaddr_in server;
    struct pdu request;
    socklen_t server_len = sizeof(server);

    // Create UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Cannot create socket");
        exit(1);
    }

    // Set up server address structure
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server.sin_addr);

    // Prepare the deregistration request
    request.type = DEREGISTER;
    strncpy(request.data, filename, sizeof(request.data));

    // Send the deregistration request to the index server
    sendto(sd, &request, sizeof(request), 0, (struct sockaddr *)&server, server_len);
    close(sd);

    // Remove the file from the local registry
    remove_registry_entry(filename);
}

// Starts a TCP server to serve files to requesting peers
// Parameters:
// - port: The port number to serve the file
// - filename: The name of the file to serve
void start_tcp_server(int port, const char *filename) {
    int sd, new_sd;
    struct sockaddr_in server, client;
    struct pdu request;
    socklen_t client_len;
    int n;

    // Create TCP socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Cannot create TCP socket");
        exit(1);
    }

    // Set up server address structure
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the server address
    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Cannot bind TCP socket");
        close(sd);
        exit(1);
    }

    // Listen for incoming connections
    listen(sd, 5);
    printf("\nMessage: TCP Server listening on port %d to serve file: %s\n", port, filename);

    // Infinite loop to handle incoming connections
    while (1) {
        client_len = sizeof(client);
        // Accept a connection from a client
        new_sd = accept(sd, (struct sockaddr *)&client, &client_len);
        if (new_sd < 0) {
            perror("Accept failed");
            continue;
        }

        // Read the request from the client
        if ((n = read(new_sd, &request, sizeof(request))) > 0 && request.type == DOWNLOAD) {
            printf("File request received for: %s\n", filename);
            FILE *file = fopen(filename, "rb");
            if (!file) {
                perror("File not found");
                struct pdu error_pdu = { ERROR, "File not found" };
                write(new_sd, &error_pdu, sizeof(error_pdu));
                close(new_sd);
                continue;
            }

            // Prepare and send the file in chunks
            struct pdu file_pdu;
            file_pdu.type = CONTENT_DATA;
            while ((n = fread(file_pdu.data, 1, sizeof(file_pdu.data), file)) > 0) {
                write(new_sd, &file_pdu, n + sizeof(file_pdu.type));  // Send only data read plus type
            }
            fclose(file);

            // Send final packet with only the type set to FINAL
            struct pdu end_pdu = { FINAL, {0} };
            write(new_sd, &end_pdu, sizeof(end_pdu));
        }
        close(new_sd);
    }
    close(sd);
}

// Downloads a file from a peer
// Parameters:
// - peer_ip: IP address of the peer hosting the file
// - peer_port: Port number of the peer
// - filename: The name of the file to download
void download_file(const char *peer_ip, int peer_port, const char *filename) {
    int sd;
    struct sockaddr_in server;
    struct pdu request, response;
    int n;

    // Create TCP socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Cannot create TCP socket");
        exit(1);
    }

    // Set up server address structure
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip, &server.sin_addr);

    // Connect to the peer server
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Cannot connect to peer server");
        close(sd);
        exit(1);
    }

    // Send a download request
    request.type = DOWNLOAD;
    strncpy(request.data, filename, sizeof(request.data));
    write(sd, &request, sizeof(request));

    // Open a file to write the downloaded data
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        close(sd);
        return;
    }

    // Receive the file from the peer server
    while ((n = read(sd, &response, sizeof(response))) > 0) {
        if (response.type == CONTENT_DATA) {
            // Write only the actual data without any padding
            int data_size = n - sizeof(response.type);
            if (data_size > 0) {
                fwrite(response.data, 1, data_size, file);
            }
        } else if (response.type == FINAL) {
            printf("File transfer complete\n");
            break;  // End of file transfer
        } else if (response.type == ERROR) {
            printf("Error: %s\n", response.data);
            fclose(file);
            remove(filename);  // Remove incomplete file
            break;
        }
        // Clear the response data to prevent residual data
        memset(response.data, 0, sizeof(response.data));
    }

    fclose(file);
    close(sd);
}

// TCP server thread function
// Parameters:
// - args: Arguments to pass to the server thread (port and filename)
void *tcp_server_thread(void *args) {
    ServerArgs *server_args = (ServerArgs *)args;
    start_tcp_server(server_args->port, server_args->filename);
    free(server_args);
    return NULL;
}

// Searches for a file and lists active peers with the file
// Parameters:
// - server_ip: IP address of the index server
// - server_port: Port number of the index server
// - filename: The name of the file to search
void search_content(const char *server_ip, int server_port, const char *filename) {
    int sd;
    struct sockaddr_in server;
    struct pdu request, response;
    socklen_t server_len = sizeof(server);

    // Create UDP socket
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Cannot create socket");
        exit(1);
    }

    // Set up server address structure
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server.sin_addr);

    // Prepare the search request
    request.type = SEARCH;
    strncpy(request.data, filename, sizeof(request.data));

    // Send the search request to the index server
    if (sendto(sd, &request, sizeof(request), 0, (struct sockaddr *)&server, server_len) == -1) {
        perror("Failed to send search request");
        close(sd);
        return;
    }

    // Receive the response from the server
    if (recvfrom(sd, &response, sizeof(response), 0, (struct sockaddr *)&server, &server_len) == -1) {
        perror("Failed to receive response");
    } else if (response.type == LIST_CONTENT) {
        printf("Peers with file '%s': %s\n", filename, response.data);
    } else if (response.type == ERROR) {
        printf("Error: %s\n", response.data);
    }

    close(sd);
}

// Entry point of the P2P client
// Parameters:
// - argc: Number of command-line arguments
// - argv: Array of command-line arguments
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <index_server_ip> <index_server_port>\n", argv[0]);
        exit(1);
    }

    char *index_server_ip = argv[1];
    int index_server_port = atoi(argv[2]);

    char command[20];
    while (1) {
        printf("\nEnter a command (register, download, search, deregister, or exit): ");
        scanf("%s", command);

        if (strcmp(command, "register") == 0) {
            char filename[100];
            int port;
            printf("Enter filename to register: ");
            scanf("%s", filename);
            printf("Enter port for serving the file: ");
            scanf("%d", &port);

            if (is_file_registered(filename)) {
                printf("File '%s' is already registered.\n", filename);
                continue;
            }

            register_content(index_server_ip, index_server_port, filename, port);

            pthread_t thread_id;
            ServerArgs *args = malloc(sizeof(ServerArgs));
            args->port = port;
            strncpy(args->filename, filename, sizeof(args->filename) - 1);
            pthread_create(&thread_id, NULL, tcp_server_thread, args);
            pthread_detach(thread_id);

            add_registry_entry(filename, port, thread_id);

        } else if (strcmp(command, "download") == 0) {
            char peer_ip[INET_ADDRSTRLEN];
            int peer_port;
            char filename[100];
            printf("Enter peer IP: ");
            scanf("%s", peer_ip);
            printf("Enter peer port: ");
            scanf("%d", &peer_port);
            printf("Enter filename to download: ");
            scanf("%s", filename);

            download_file(peer_ip, peer_port, filename);

            int port = get_random_port();

            register_content(index_server_ip, index_server_port, filename, port);

            pthread_t thread_id;
            ServerArgs *args = malloc(sizeof(ServerArgs));
            args->port = port;
            strncpy(args->filename, filename, sizeof(args->filename) - 1);
            pthread_create(&thread_id, NULL, tcp_server_thread, args);
            pthread_detach(thread_id);

        } else if (strcmp(command, "deregister") == 0) {
            char filename[100];
            printf("Enter filename to deregister: ");
            scanf("%s", filename);

            deregister_content(index_server_ip, index_server_port, filename);

        } else if (strcmp(command, "search") == 0) {
            char filename[100];
            printf("Enter filename to search: ");
            scanf("%s", filename);

            // Search for peers with the specified file
            search_content(index_server_ip, index_server_port, filename);

        } else if (strcmp(command, "exit") == 0) {
            printf("Exiting and cleaning up...\n");
            cleanup_on_exit(index_server_ip, index_server_port);
            break;

        } else {
            printf("Unknown command. Please enter 'register', 'download', 'search', 'deregister', or 'exit'.\n");
        }
    }

    return 0;
}
