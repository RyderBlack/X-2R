#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"      // Platform-specific includes
#include "env_loader.h"
#include "db_connection.h"

// Platform-specific networking
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    WSADATA wsaData;  // Local declaration for Winsock
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Load environment variables
    if (!load_env(".env")) {
        fprintf(stderr, "Failed to load .env file\n");
        return EXIT_FAILURE;
    }

    // Connect to PostgreSQL
    PGconn *conn = connect_to_db();
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return EXIT_FAILURE;
    }

    INIT_NETWORKING();

    // Socket setup
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        CLOSE_SOCKET(server_fd);
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        CLOSE_SOCKET(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        CLOSE_SOCKET(server_fd);
        return EXIT_FAILURE;
    }

    printf("✅ Server is listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
#ifdef _WIN32
        struct sockaddr_in sa;
        int sa_len = sizeof(sa);
        getpeername(new_socket, (struct sockaddr*)&sa, &sa_len);
        inet_ntop(AF_INET, &sa.sin_addr, client_ip, sizeof(client_ip));
#else
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
#endif

        printf("📥 Connection accepted from %s:%d\n", client_ip, ntohs(address.sin_port));

        int bytes_read = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("📝 Received: %s\n", buffer);

            // Echo back the received message
            send(new_socket, buffer, strlen(buffer), 0);
            printf("📤 Echoed back to client.\n");

            // Future enhancement: insert into DB
        }

        CLOSE_SOCKET(new_socket);
    }

    PQfinish(conn);
    CLOSE_SOCKET(server_fd);
    CLEANUP_NETWORKING();

    return EXIT_SUCCESS;
}
