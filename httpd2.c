#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <sys/errno.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

/* Function to determine the content based on the requested resource*/
const char* get_content_for_resource(const char *resource) {
    if (strcmp(resource, "/") == 0) {
        return "<html><body><h1>Home Page</h1>"
               "<form action=\"/submit\" method=\"POST\">"
               "Name: <input type=\"text\" name=\"name\"><br>"
               "<input type=\"submit\" value=\"Submit\">"
               "</form></body></html>";
    } else if (strcmp(resource, "/about") == 0) {
        return "<html><body><h1>About Page</h1></body></html>";
    } else if (strcmp(resource, "/contact") == 0) {
        return "<html><body><h1>Contact Page</h1></body></html>";
    } else {
        return NULL;  /* Return NULL to indicate that the resource should be served from a file or dynamically generated */
    }
}

/* Function to parse query strings */
void parse_query_string(char *query_string, char params[][2][BUFFER_SIZE], int *param_count) {
    char *start = query_string;
    *param_count = 0;

    while (start && *start) {
        char *eq = strchr(start, '=');
        char *amp = strchr(start, '&');

        if (eq) {
            int key_len = eq - start;
            int value_len = amp ? amp - eq - 1 : strlen(eq + 1);

            strncpy(params[*param_count][0], start, key_len);
            params[*param_count][0][key_len] = '\0';
            strncpy(params[*param_count][1], eq + 1, value_len);
            params[*param_count][1][value_len] = '\0';

            (*param_count)++;
        }

        start = amp ? amp + 1 : NULL;
    }
}

const char* handle_form_submission(const char *data) {
    static char response[BUFFER_SIZE];
    char name[BUFFER_SIZE];

    /* Simple parsing: find the value for the "name" field */
    if (sscanf(data, "name=%s", name) == 1) {
/*         Replace '+' with ' ' in the name (HTTP encoding)
        char *p;
        for (*p = name; *p; ++p) {
            if (*p == '+') *p = ' ';
        } */

        int i;
        for (i = 0; name[i] != '\0'; i++) {
        if (name[i] == '+') {
            name[i] = ' ';
        }
    }
        snprintf(response, BUFFER_SIZE, "<html><body><h1>Hello, %s!</h1></body></html>", name);
    } else {
        snprintf(response, BUFFER_SIZE, "<html><body><h1>Error in submission</h1></body></html>");
    }

    return response;
}

int serve_file_from_disk(const char *filepath, int client_socket) {
    struct stat file_stat;
    char header[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) {
        perror("Error opening file");
        return -1;  /*  File not found or error */
    }

    if (fstat(file_fd, &file_stat) == -1) {
        perror("Error getting file size");
        close(file_fd);
        return -1;
    }

    /* Create the HTTP header */

    snprintf(header, BUFFER_SIZE, "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: %ld\n\n", file_stat.st_size);
    write(client_socket, header, strlen(header));

    /* Read the file contents in chunks and send to client */

    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        write(client_socket, file_buffer, bytes_read);
    }

    if (bytes_read == -1) {
        perror("Error reading file");
        close(file_fd);
        return -1;
    }

    close(file_fd);
    return 0;  /* Success */
}

const char* generate_dynamic_content() {
    static char dynamic_content[BUFFER_SIZE];

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    int random_number = rand() % 100;

    snprintf(dynamic_content, BUFFER_SIZE,
             "<html><body><h1>Dynamic Content</h1>"
             "<p>Current Date and Time: %02d-%02d-%04d %02d:%02d:%02d</p>"
             "<p>Random Number: %d</p>"
             "</body></html>",
             t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
             t->tm_hour, t->tm_min, t->tm_sec,
             random_number);

    return dynamic_content;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    /* int addrlen = sizeof(address); */
    char buffer[BUFFER_SIZE];
    /* HTTP response header */
    char response[BUFFER_SIZE];

    memset(buffer, 0, sizeof(buffer));

    srand(time(NULL));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server v2 is listening on port %d\n", PORT);

    while (1) {
        int valread;
        char method[16], resource[256], query_string[BUFFER_SIZE];
        char *query_start;
        struct sockaddr_in client_address;
        socklen_t client_len;

        client_len = sizeof(client_address);
        memset(query_string, 0, sizeof(query_string));

        /* if ((new_socket = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t*)&client_len)) < 0) {*/
        if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
            perror("Accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        /* Read the incoming request */
        valread = read(new_socket, buffer, BUFFER_SIZE - 1);
        buffer[valread] = '\0';  /* Null-terminate the request */

        /* Parse the request line (method, resource) */

        sscanf(buffer, "%s %s", method, resource);

        /* Separate the resource from the query string, if present */
        query_start = strchr(resource, '?');
        if (query_start) {
            strcpy(query_string, query_start + 1);
            *query_start = '\0';  /* Terminate resource string before the query string */
        }

        printf("Received request: Method = %s, Resource = %s, Query String = %s\n", method, resource, query_string);

        if (strcmp(method, "POST") == 0 && strcmp(resource, "/submit") == 0) {
            char *body;
            printf("Handling POST request to \"/submit\"\n");
            /* Find the start of the POST data (after the headers) */
            body = strstr(buffer, "\r\n\r\n");
            if (body) {
                const char *html_content;
                int content_length;
                body += 4;  /* Move past the "\r\n\r\n" */

                html_content = handle_form_submission(body);
                content_length = strlen(html_content);
                snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n%s", content_length, html_content);
                write(new_socket, response, strlen(response));
            } else {
                const char *error_content = "<html><body><h1>Error in submission</h1></body></html>";
                int content_length = strlen(error_content);
                snprintf(response, BUFFER_SIZE, "HTTP/1.1 400 Bad Request\nContent-Type: text/html\nContent-Length: %d\n\n%s", content_length, error_content);
                write(new_socket, response, strlen(response));
            }
        } else if (strcmp(resource, "/dynamic") == 0) {
            /* Serve dynamically generated content */
            const char *dynamic_content = generate_dynamic_content();
            int content_length = strlen(dynamic_content);
            snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n%s", content_length, dynamic_content);
            write(new_socket, response, strlen(response));
        } else {
            /* Handle GET requests for specific resources or files */
            const char *html_content = get_content_for_resource(resource);

            if (html_content) {
                /* Serve predefined HTML content */
                int content_length = strlen(html_content);
                snprintf(response, BUFFER_SIZE, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n%s", content_length, html_content);

                /* Parse and handle query strings for special cases */
                if (strlen(query_string) > 0) {
                    char params[10][2][BUFFER_SIZE];
                    int param_count = 0;
                    int i;
                    parse_query_string(query_string, params, &param_count);

                    /* Example: Check for a specific parameter in the query string */

                    for (i = 0; i < param_count; i++) {
                        if (strcmp(params[i][0], "name") == 0) {
                            snprintf(response + strlen(response), BUFFER_SIZE - strlen(response), "<p>Hello, %s!</p>", params[i][1]);
                        }
                    }
                }

                write(new_socket, response, strlen(response));
            } else {
                /* Serve file from disk */
                char filepath[BUFFER_SIZE];  
                strcpy(filepath, "./"); /* Assuming the files are in the current directory */
                strcat(filepath, resource + 1);  /* Skip the leading '/' */

                if (serve_file_from_disk(filepath, new_socket) == -1) {
                    /* If the file wasn't found, send a 404 response */
                    const char *not_found_content = "<html><body><h1>404 Not Found</h1></body></html>";
                    int content_length = strlen(not_found_content);
                    snprintf(response, BUFFER_SIZE, "HTTP/1.1 404 Not Found\nContent-Type: text/html\nContent-Length: %d\n\n%s", content_length, not_found_content);
                    write(new_socket, response, strlen(response));
                }
            }
        }

        close(new_socket);
    }

    return 0;
}