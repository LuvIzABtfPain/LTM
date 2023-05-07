#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

#define MAX_CLIENTS 10
#define BUF_SIZE 1024

struct client_info {
    int fd;
    char name[256];
};

int main(int argc, char *argv[]) {
    int server_fd, client_fds[MAX_CLIENTS], max_fd;
    struct sockaddr_in server_addr, client_addr;
    char buf[BUF_SIZE];
    int bytes_read;
    fd_set read_fds;
    struct timeval timeout;
    int i, j, num_clients = 0;
    struct client_info clients[MAX_CLIENTS];

    // Check for correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));

    // Bind socket to server address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening for incoming connections on port %s\n", argv[1]);

    // Initialize client file descriptors
    for (i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;
    }

    // Initialize file descriptor set
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    max_fd = server_fd;

    while (1) {
        // Wait for activity on one of the file descriptors
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        if (select(max_fd + 1, &read_fds, NULL, NULL, &timeout) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Check for activity on server socket
        if (FD_ISSET(server_fd, &read_fds)) {
            // Accept incoming connection
            socklen_t client_addr_size = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_size);
            if (client_fd == -1) {
                perror("accept");
                continue;
            }

            printf("Accepted incoming connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // Add client to list
            if (num_clients == MAX_CLIENTS) {
                fprintf(stderr, "Too many clients\n");
                close(client_fd);
            } else {
                // Add client to list of file descriptors
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (client_fds[i] == -1) {
                        client_fds[i] = client_fd;
                        break;
                    }
                }

                // Add client to list of client info
                struct client_info client_info = {client_fd, ""};
                clients[num_clients++] = client_info;

                printf("Current clients:\n");
                for (i = 0; i < num_clients; i++) {
                    printf("  %s\n", clients[i].name);
                }
            }
        }

        // Check for activity on client sockets
        for (i = 0; i < num_clients; i++) {
            if (FD_ISSET(clients[i].fd, &read_fds)) {
                // Read data from client
                bytes_read = read(clients[i].fd, buf, BUF_SIZE);
                if (bytes_read == -1) {
                    perror("read");
                    continue;
                } else if (bytes_read == 0) {
                    // Client disconnected
                    printf("%s disconnected\n", clients[i].name);
                    close(clients[i].fd);

                    // Remove client from list of file descriptors
                    for (j = 0; j < MAX_CLIENTS; j++) {
                        if (client_fds[j] == clients[i].fd) {
                            client_fds[j] = -1;
                            break;
                        }
                    }

                    // Remove client from list of client info
                    for (j = i; j < num_clients - 1; j++) {
                        clients[j] = clients[j + 1];
                    }
                    num_clients--;

                    printf("Current clients:\n");
                    for (j = 0; j < num_clients; j++) {
                        printf("  %s\n", clients[j].name);
                    }
                } else {
                    // Parse client message
                    buf[bytes_read] = '\0';
                    char *colon_pos = strchr(buf, ':');
                    if (colon_pos == NULL) {
                        // Invalid message syntax
                        printf("Invalid message syntax from %s\n", clients[i].name);
                        continue;
                    }
                    *colon_pos = '\0';
                    char *client_id = buf;
                    char *message = colon_pos + 1;

                    // Update client name
                    strncpy(clients[i].name, message, sizeof(clients[i].name) - 1);
                    printf("%s is now known as %s\n", client_id, clients[i].name);

                    // Send message to all other clients
                    printf("%s: %s\n", clients[i].name, message);
                    for (j = 0; j < num_clients; j++) {
                        if (i != j) {
                            write(clients[j].fd, buf, strlen(buf));
                        }
                    }
                }
            }
        }

        // Reset file descriptor set
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;
        for (i = 0; i < num_clients; i++) {
            FD_SET(clients[i].fd, &read_fds);
            if (clients[i].fd > max_fd) {
                max_fd = clients[i].fd;
            }
        }
    }

    return 0;
}
