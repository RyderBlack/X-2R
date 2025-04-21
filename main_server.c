#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "env_loader.h"
#include "db_connection.h"

#define PORT 12345
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Load environment
    if (!load_env(".env")) {
        fprintf(stderr, "Failed to load .env file\n");
        return EXIT_FAILURE;
    }

    // Connect to the database
    PGconn *conn = connect_db();
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return EXIT_FAILURE;
    }

    // Setup server socket
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // Bind socket to the port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("✅ Server is listening on port %d...\n", PORT);

    // Accept loop
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        printf("📥 Connection accepted.\n");

        // Read incoming data
        int bytes_read = read(new_socket, buffer, BUFFER_SIZE);
        buffer[bytes_read] = '\0';
        printf("📝 Received: %s\n", buffer);

        // Echo back
        send(new_socket, buffer, strlen(buffer), 0);
        printf("📤 Echoed back to client.\n");

        close(new_socket);
    }

    PQfinish(conn);
    close(server_fd);
    return EXIT_SUCCESS;
}
