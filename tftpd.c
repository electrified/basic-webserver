#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>

#define TFTP_DATA_SIZE 512
#define TFTP_TIMEOUT 5
#define TFTP_OPCODE_RRQ 1
#define TFTP_OPCODE_WRQ 2
#define TFTP_OPCODE_DATA 3
#define TFTP_OPCODE_ACK 4
#define TFTP_OPCODE_ERROR 5

// TFTP error codes
#define TFTP_ERROR_FILE_NOT_FOUND 1
#define TFTP_ERROR_ACCESS_VIOLATION 2
#define TFTP_ERROR_DISK_FULL 3
#define TFTP_ERROR_ILLEGAL_OP 4
#define TFTP_ERROR_UNKNOWN_ID 5

#define DEFAULT_TFTP_PORT 69 // Default TFTP port
#define MAX_PATH_LENGTH 1024  // Maximum path length for files

// Packet buffer size
#define PACKET_SIZE (4 + TFTP_DATA_SIZE)

// Function declarations
void handle_rrq(int sock, struct sockaddr_in *client_addr, socklen_t client_len, const char *filename, const char *directory);
void handle_wrq(int sock, struct sockaddr_in *client_addr, socklen_t client_len, const char *filename, const char *directory);
void send_error(int sock, struct sockaddr_in *client_addr, socklen_t client_len, int error_code, const char *error_msg);
void log_request(const struct sockaddr_in *client_addr, const char *operation, const char *filename);
void log_error(const char *message, const struct sockaddr_in *client_addr, int error_code, const char *error_msg);
void log_packet(const char *action, const char *type, const char *filename, uint16_t block, ssize_t length);

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[PACKET_SIZE];
    ssize_t recv_len;
    int port = DEFAULT_TFTP_PORT; // Default TFTP port
    const char *directory = ".";   // Default directory is current working directory

    // Check for command-line arguments
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <port> [directory]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Set port from command-line argument
    port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %s. Using default port %d.\n", argv[1], DEFAULT_TFTP_PORT);
        port = DEFAULT_TFTP_PORT;
    }

    // Set directory from command-line argument if provided
    if (argc == 3) {
        directory = argv[2];
    }

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind to specified port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("TFTP server listening on port %d, using directory: '%s'...\n", port, directory);

    while (1) {
        // Receive incoming TFTP request
        recv_len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
        if (recv_len < 0) {
            perror("recvfrom failed");
            continue;
        }

        // Determine the request type (RRQ or WRQ)
        uint16_t opcode = ntohs(*(uint16_t *)buffer);
        char *filename = buffer + 2;

        // Log incoming packet
        log_packet("Received", opcode == TFTP_OPCODE_RRQ ? "RRQ" : "WRQ", filename, 0, recv_len);

        if (opcode == TFTP_OPCODE_RRQ) {
            log_request(&client_addr, "RRQ", filename);
            handle_rrq(sock, &client_addr, client_len, filename, directory);
        } else if (opcode == TFTP_OPCODE_WRQ) {
            log_request(&client_addr, "WRQ", filename);
            handle_wrq(sock, &client_addr, client_len, filename, directory);
        } else {
            // Unsupported request type
            log_error("Unsupported TFTP operation", &client_addr, TFTP_ERROR_ILLEGAL_OP, "Illegal TFTP operation");
            send_error(sock, &client_addr, client_len, TFTP_ERROR_ILLEGAL_OP, "Illegal TFTP operation");
        }
    }

    close(sock);
    return 0;
}
void handle_rrq(int sock, struct sockaddr_in *client_addr, socklen_t client_len, const char *filename, const char *directory) {
    char data_packet[PACKET_SIZE];
    char full_path[MAX_PATH_LENGTH];
    char resolved_path[PATH_MAX];

    // Construct the full file path
    snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);

    // Resolve the absolute path
    if (realpath(full_path, resolved_path) == NULL) {
        log_error("Could not resolve path", client_addr, TFTP_ERROR_ACCESS_VIOLATION, strerror(errno));
        send_error(sock, client_addr, client_len, TFTP_ERROR_ACCESS_VIOLATION, "Access violation: Invalid path");
        return;
    }

    // Check if the resolved path starts with the expected directory path
    if (strncmp(resolved_path, directory, strlen(directory)) != 0) {
        log_error("Access violation: Attempt to read outside designated directory", client_addr, TFTP_ERROR_ACCESS_VIOLATION, "Access violation: Path traversal detected");
        send_error(sock, client_addr, client_len, TFTP_ERROR_ACCESS_VIOLATION, "Access violation: Path traversal detected");
        return;
    }

    // Open file for reading
    int file = open(resolved_path, O_RDONLY);
    if (file < 0) {
        log_error("File cannot be opened", client_addr, TFTP_ERROR_FILE_NOT_FOUND, strerror(errno));
        send_error(sock, client_addr, client_len, TFTP_ERROR_FILE_NOT_FOUND, "File not found");
        return;
    }

    printf("RRQ: Sending file '%s' to client\n", resolved_path);

    uint16_t block = 0;
    ssize_t bytes_read;

    // Send data blocks to client
    while ((bytes_read = read(file, data_packet + 4, TFTP_DATA_SIZE)) > 0) {
        // Prepare the data packet
        data_packet[0] = 0;
        data_packet[1] = TFTP_OPCODE_DATA;
        data_packet[2] = (block + 1) >> 8;
        data_packet[3] = (block + 1) & 0xFF;

        // Send the data packet
        if (sendto(sock, data_packet, bytes_read + 4, 0, (struct sockaddr *)client_addr, client_len) < 0) {
            perror("sendto failed");
            close(file);
            return;
        }

        printf("Sent DATA block %d, Size: %zd bytes\n", block + 1, bytes_read);
        
        // Wait for the ACK from the client
        char ack_packet[4];
        socklen_t len = client_len;
        if (recvfrom(sock, ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)client_addr, &len) < 0) {
            perror("recvfrom failed");
            close(file);
            return;
        }

        // Check if the received packet is an ACK
        if (ntohs(*(uint16_t *)ack_packet) != TFTP_OPCODE_ACK || ntohs(*(uint16_t *)(ack_packet + 2)) != block + 1) {
            log_error("Invalid ACK received", client_addr, TFTP_ERROR_ILLEGAL_OP, "Illegal TFTP operation");
            send_error(sock, client_addr, client_len, TFTP_ERROR_ILLEGAL_OP, "Illegal TFTP operation");
            close(file);
            return;
        }

        block++;
    }

    close(file);
    printf("RRQ: File '%s' send complete\n", resolved_path);
}

void handle_wrq(int sock, struct sockaddr_in *client_addr, socklen_t client_len, const char *filename, const char *directory) {
    char data_packet[PACKET_SIZE];
    char full_path[MAX_PATH_LENGTH];
    char resolved_path[PATH_MAX];

    // Construct the full file path
    snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);

    // Resolve the absolute path
    if (realpath(full_path, resolved_path) == NULL) {
        log_error("Could not resolve path", client_addr, TFTP_ERROR_ACCESS_VIOLATION, strerror(errno));
        send_error(sock, client_addr, client_len, TFTP_ERROR_ACCESS_VIOLATION, "Access violation: Invalid path");
        return;
    }

    // Check if the resolved path starts with the expected directory path
    if (strncmp(resolved_path, directory, strlen(directory)) != 0) {
        log_error("Access violation: Attempt to write outside designated directory", client_addr, TFTP_ERROR_ACCESS_VIOLATION, "Access violation: Path traversal detected");
        send_error(sock, client_addr, client_len, TFTP_ERROR_ACCESS_VIOLATION, "Access violation: Path traversal detected");
        return;
    }

    // Open file for writing
    int file = open(resolved_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file < 0) {
        log_error("File cannot be created", client_addr, TFTP_ERROR_ACCESS_VIOLATION, strerror(errno));
        send_error(sock, client_addr, client_len, TFTP_ERROR_ACCESS_VIOLATION, "Cannot create file");
        return;
    }

    printf("WRQ: Receiving file '%s' from client\n", resolved_path);

    // Send ACK for block 0
    data_packet[0] = 0;
    data_packet[1] = TFTP_OPCODE_ACK;
    data_packet[2] = 0;
    data_packet[3] = 0;
    if (sendto(sock, data_packet, 4, 0, (struct sockaddr *)client_addr, client_len) < 0) {
        perror("sendto failed");
        close(file);
        return;
    }
    printf("Sent ACK for block 0\n");

    // Receive data and write to file
    ssize_t recv_len;
    uint16_t block = 0;
    while ((recv_len = recvfrom(sock, data_packet, sizeof(data_packet), 0, (struct sockaddr *)client_addr, &client_len)) > 0) {
        uint16_t data_block = ntohs(*(uint16_t *)(data_packet + 2));
        if (ntohs(*(uint16_t *)data_packet) != TFTP_OPCODE_DATA) {
            log_error("Not a DATA packet", client_addr, TFTP_ERROR_ILLEGAL_OP, "Illegal TFTP operation");
            send_error(sock, client_addr, client_len, TFTP_ERROR_ILLEGAL_OP, "Illegal TFTP operation");
            close(file);
            return;
        }

        if (data_block == block + 1) {
            // Write data to file
            write(file, data_packet + 4, recv_len - 4);

            // Send ACK for the received block
            char ack_packet[4];
            ack_packet[0] = 0;
            ack_packet[1] = TFTP_OPCODE_ACK;
            ack_packet[2] = (block + 1) >> 8;
            ack_packet[3] = (block + 1) & 0xFF;

            if (sendto(sock, ack_packet, 4, 0, (struct sockaddr *)client_addr, client_len) < 0) {
                perror("sendto failed");
                close(file);
                return;
            }

            printf("Sent ACK for block %d\n", block + 1);
            block++;
        }

        // End of transfer (last block < 512 bytes)
        if (recv_len < TFTP_DATA_SIZE + 4) {
            break;
        }
    }

    close(file);
    printf("WRQ: File '%s' upload complete\n", resolved_path);
}


void send_error(int sock, struct sockaddr_in *client_addr, socklen_t client_len, int error_code, const char *error_msg) {
    char error_packet[516];
    *(uint16_t *)error_packet = htons(TFTP_OPCODE_ERROR);
    *(uint16_t *)(error_packet + 2) = htons(error_code);
    strcpy(error_packet + 4, error_msg);

    printf("Sending ERROR: Code %d, Message: '%s'\n", error_code, error_msg);

    sendto(sock, error_packet, strlen(error_msg) + 5, 0, (struct sockaddr *)client_addr, client_len);
}

void log_request(const struct sockaddr_in *client_addr, const char *operation, const char *filename) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[%s:%d] %s request for file: '%s'\n", client_ip, ntohs(client_addr->sin_port), operation, filename);
}

void log_error(const char *message, const struct sockaddr_in *client_addr, int error_code, const char *error_msg) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("ERROR: [%s:%d] %s (Code %d: %s)\n", client_ip, ntohs(client_addr->sin_port), message, error_code, error_msg);
}

void log_packet(const char *action, const char *type, const char *filename, uint16_t block, ssize_t length) {
    printf("%s: %s for '%s', Block: %d, Length: %zd bytes\n", action, type, filename, block, length);
}
