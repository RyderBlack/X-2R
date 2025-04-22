#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define CLOSESOCKET closesocket
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #define CLOSESOCKET close
#endif

#include "platform.h"
#include "env_loader.h"
#include "db_connection.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Load environment
    if (!load_env(".env")) {
        fprintf(stderr, "Failed to load .env file\n");
        return EXIT_FAILURE;
    }

    // Connect to the database
    PGconn *conn = connect_to_db();
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return EXIT_FAILURE;
    }
#endif

    // Setup server socket
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        CLOSESOCKET(server_fd);
        return EXIT_FAILURE;
    }

    // Bind socket to the port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        CLOSESOCKET(server_fd);
        return EXIT_FAILURE;
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        CLOSESOCKET(server_fd);
        return EXIT_FAILURE;
    }

    printf("✅ Server is listening on port %d...\n", PORT);

    // Accept loop
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        printf("📥 Connection accepted.\n");

        // Read incoming data
        int bytes_read = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("📝 Received: %s\n", buffer);

            // Echo back
            send(new_socket, buffer, strlen(buffer), 0);
            printf("📤 Echoed back to client.\n");
        }

        CLOSESOCKET(new_socket);
    }

    PQfinish(conn);
    CLOSESOCKET(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}