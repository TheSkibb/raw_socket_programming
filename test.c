#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/example_socket"
#define BUFFER_SIZE 100

int main() {
    int server_fd, client_fd;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[BUFFER_SIZE];

    // Remove the socket file if it already exists
    unlink(SOCKET_PATH);

    // Create a Unix domain socket
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on %s...\n", SOCKET_PATH);

    // Main loop to accept and handle client connections
    while (1) {
        client_addr_len = sizeof(client_addr);
        // Accept a connection from a client
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue; // If accept fails, go back to listening
        }

        printf("Client connected\n");

        // Send a message to the client
        const char *message = "You are connected to the Unix socket server.\n";
        write(client_fd, message, strlen(message));

        // Close the client connection
        close(client_fd);
        printf("Client disconnected\n");
    }

    // Cleanup (this part will not be reached, but it's good practice)
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}

