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

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // // Load environment
    // if (!load_env(".env")) {
    //     fprintf(stderr, "Failed to load .env file\n");
    //     return EXIT_FAILURE;
    // }

    // // Connect to the database
    // PGconn *conn = connect_to_db();
    // if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
    //     fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(conn));
    //     PQfinish(conn);
    //     return EXIT_FAILURE;
    // }

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return EXIT_FAILURE;
    }
#endif

    // Socket setup
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    // Convert IP address
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        return EXIT_FAILURE;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return EXIT_FAILURE;
    }

    printf("✅ Connected to server on port %d\n", SERVER_PORT);

    // Send message
    const char *hello = "Hello from client!";
    send(sock, hello, strlen(hello), 0);
    printf("📤 Sent: %s\n", hello);

    // Receive response
    int valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("📥 Received from server: %s\n", buffer);
    }

    // Cleanup
    CLOSESOCKET(sock);
    // PQfinish(conn);

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}