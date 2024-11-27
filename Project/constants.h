// constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#define SERVER_PORT 15000    // Default server port for the index server
#define BUFLEN 256           // Buffer length for PDU
#define MAX_ENTRIES 100      // Maximum number of registered files
#define FILENAME_SIZE 11    // Maximum size for filenames
#define PEER_NAME_SIZE 11   // Maximum size for peer names

// Define PDU Types
#define REGISTER 'R'
#define DEREGISTER 'T'
#define SEARCH 'S'
#define DOWNLOAD 'D'
#define CONTENT_DATA 'C'
#define LIST_CONTENT 'O'
#define ACKNOWLEDGE 'A'
#define ERROR 'E'
#define FINAL 'F'

// PDU Data Structure
struct pdu {
    char type;
    char data[BUFLEN];
};

#endif // CONSTANTS_H
