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

void deregister_content(const char *server_ip, int server_port, const char *filename);
void *tcp_server_thread(void *args);

// Structure to pass arguments to the TCP server thread
typedef struct {
    int port;
    char filename[100];
} ServerArgs;

// Structure for file registry to track active files, ports, and thread IDs
typedef struct {
    char filename[100];
    int port;
    pthread_t thread_id;
} FileRegistryEntry;

FileRegistryEntry registry[MAX_ENTRIES];  // Array to store active file servers
int registry_count = 0;                   // Track the number of registered files

// Check if file is already registered
int is_file_registered(const char *filename) {
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(registry[i].filename, filename) == 0) {
            return 1;  // File is already registered
        }
    }
    return 0;
}

// Function to remove an entry from the registry
void remove_registry_entry(const char *filename) {
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(registry[i].filename, filename) == 0) {
            pthread_cancel(registry[i].thread_id);
            for (int j = i; j < registry_count - 1; j++) {
                registry[j] = registry[j + 1];
            }
            registry_count--;
            break;
        }
    }
}

// Function to add an entry to the registry
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

// Function to deregister all files on exit
void cleanup_on_exit(const char *server_ip, int server_port) {
    for (int i = 0; i < registry_count; i++) {
        deregister_content(server_ip, server_port, registry[i].filename);
        pthread_cancel(registry[i].thread_id);
    }
    registry_count = 0;
}

int get_random_port() {
    int min_port = 20000;
    int max_port = 65535;
    srand(time(NULL));
    return min_port + rand() % (max_port - min_port + 1);
}

// Function to register content with the index server
void register_content(const char *server_ip, int server_port, const char *filename, int tcp_port) {
    if (is_file_registered(filename)) {
        printf("File '%s' is already registered.\n", filename);
        return;
    }

    int sd;
    struct sockaddr_in server;
    struct pdu request;
    socklen_t server_len = sizeof(server);

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Cannot create socket");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server.sin_addr);

    request.type = REGISTER;
    snprintf(request.data, sizeof(request.data), "%s:%d", filename, tcp_port);

    sendto(sd, &request, sizeof(request), 0, (struct sockaddr *)&server, server_len);
    close(sd);
}

// Function to deregister content with the index server
void deregister_content(const char *server_ip, int server_port, const char *filename) {
    int sd;
    struct sockaddr_in server;
    struct pdu request;
    socklen_t server_len = sizeof(server);

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Cannot create socket");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server.sin_addr);

    request.type = DEREGISTER;
    strncpy(request.data, filename, sizeof(request.data));

    sendto(sd, &request, sizeof(request), 0, (struct sockaddr *)&server, server_len);
    close(sd);

    remove_registry_entry(filename);
}

// TCP Server for serving files to requesting peers
void start_tcp_server(int port, const char *filename) {
    int sd, new_sd;
    struct sockaddr_in server, client;
    struct pdu request;
    socklen_t client_len;
    int n;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Cannot create TCP socket");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Cannot bind TCP socket");
        close(sd);
        exit(1);
    }

    listen(sd, 5);
    printf("\nMessage: TCP Server listening on port %d to serve file: %s\n", port, filename);

    while (1) {
        client_len = sizeof(client);
        new_sd = accept(sd, (struct sockaddr *)&client, &client_len);
        if (new_sd < 0) {
            perror("Accept failed");
            continue;
        }

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

            struct pdu file_pdu;
            file_pdu.type = CONTENT_DATA;
            while ((n = fread(file_pdu.data, 1, sizeof(file_pdu.data), file)) > 0) {
                write(new_sd, &file_pdu, n + sizeof(file_pdu.type));  // Send only data read plus type
            }
            fclose(file);

            // Send final packet with only the type set to FINAL
            struct pdu end_pdu = { FINAL, {0} };
            while ((n = fread(file_pdu.data, 1, sizeof(file_pdu.data), file)) > 0) {
                write(new_sd, &file_pdu, n + sizeof(file_pdu.type));  // Send only data read plus type
            }

        }
        close(new_sd);
    }
    close(sd);
}

// TCP Client for downloading a file from a peer
void download_file(const char *peer_ip, int peer_port, const char *filename) {
    int sd;
    struct sockaddr_in server;
    struct pdu request, response;
    int n;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Cannot create TCP socket");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip, &server.sin_addr);

    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Cannot connect to peer server");
        close(sd);
        exit(1);
    }

    request.type = DOWNLOAD;
    strncpy(request.data, filename, sizeof(request.data));
    write(sd, &request, sizeof(request));

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        close(sd);
        return;
    }

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
void *tcp_server_thread(void *args) {
    ServerArgs *server_args = (ServerArgs *)args;
    start_tcp_server(server_args->port, server_args->filename);
    free(server_args);
    return NULL;
}

// Main function
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
