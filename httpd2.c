#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // HTTP response header and body
    char *http_header = "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n%s";
    char *html_content = "<html><body><h1>Hello, World!</h1></body></html>";
    char response[BUFFER_SIZE];

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Define the server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    // Accept incoming connections and respond with HTTP response
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        // Read the incoming request (not really used in this simple server)
        read(new_socket, buffer, BUFFER_SIZE);
        
        // Create the HTTP response
        int content_length = strlen(html_content);
        snprintf(response, BUFFER_SIZE, http_header, content_length, html_content);

        // Send the response to the client
        write(new_socket, response, strlen(response));

        // Close the connection
        close(new_socket);
    }

    return 0;
}
