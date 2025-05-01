#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> // <-- NEW!
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

#include "network/platform.h"
#include "config/env_loader.h"
#include "database/db_connection.h"
#include "network/protocol.h"

#define PORT 8080
#define BUFFER_SIZE 1024

// Structure to pass data to client handler thread
typedef struct {
    int socket;
    PGconn *db_conn;
} ClientData;

// Thread function for handling a client
void* handle_client(void* arg) {
    ClientData *data = (ClientData *)arg;
    int client_socket = data->socket;
    PGconn *conn = data->db_conn;
    free(data);

    printf("📥 New thread started for client socket %d\n", client_socket);

    while (1) {
        Message* msg = receive_message(client_socket);
        if (!msg) {
            printf("❌ Client disconnected or error on socket %d\n", client_socket);
            break;
        }

        printf("📝 Received message type: %d, length: %u\n", msg->type, msg->length);

        if (msg->type == MSG_CHAT) {
            ChatMessage* chat = (ChatMessage*)msg->payload;
            printf("💬 [Channel %u] %s: %s\n", chat->channel_id, chat->sender_username, chat->content);

            // Echo message back to client
            send_message(client_socket, msg);
            printf("📤 Echoed chat message back to client.\n");
        } else {
            printf("❓ Received non-chat message, ignoring.\n");
        }

        free(msg);
    }

    CLOSESOCKET(client_socket);
    printf("🔒 Connection closed for socket %d\n", client_socket);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // Load environment variables from the .env file
    if (!load_env(".env")) {
        fprintf(stderr, "Failed to load .env file\n");
        return EXIT_FAILURE;
    }

    PGconn *conn = connect_to_db();
    if (conn == NULL || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Database connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return EXIT_FAILURE;
    }
#endif

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        CLOSESOCKET(server_fd);
        return EXIT_FAILURE;
    }

    // Retrieve the server IP from the environment variable
    const char *server_ip = getenv("SERVER_IP");
    if (!server_ip) {
        fprintf(stderr, "SERVER_IP environment variable not set!\n");
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    // Convert the server IP string to a usable format
    if (inet_pton(AF_INET, server_ip, &address.sin_addr) <= 0) {
        perror("inet_pton failed");
        CLOSESOCKET(server_fd);
        return EXIT_FAILURE;
    }

    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        CLOSESOCKET(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        CLOSESOCKET(server_fd);
        return EXIT_FAILURE;
    }

    printf("✅ Server is listening on IP %s and port %d...\n", server_ip, PORT);

    while (1) {
        ClientData *data = malloc(sizeof(ClientData));
        if (!data) {
            perror("malloc failed");
            continue;
        }

        data->socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (data->socket < 0) {
            perror("accept");
            free(data);
            continue;
        }
        data->db_conn = conn;

        printf("🔗 Accepted connection, socket %d\n", data->socket);

        // Create thread to handle client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, data) != 0) {
            perror("pthread_create failed");
            CLOSESOCKET(data->socket);
            free(data);
            continue;
        }

        pthread_detach(thread_id);
    }

    PQfinish(conn);
    CLOSESOCKET(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}
