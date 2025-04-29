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

#include "platform.h"
#include "env_loader.h"
#include "db_connection.h"
#include "protocol.h" // Message, ChatMessage, send_message, receive_message

#define PORT 8080
#define BUFFER_SIZE 1024

// Thread function for handling a client
void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg); // We malloced it in main(), free it here

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

            // Echo the chat message back to the same client
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

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
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

    printf("✅ Server is listening on port %d...\n", PORT);

    while (1) {
        int* new_socket = malloc(sizeof(int)); // Allocate socket for thread
        if (!new_socket) {
            perror("malloc failed");
            continue;
        }

        *new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*new_socket < 0) {
            perror("accept");
            free(new_socket);
            continue;
        }

        printf("🔗 Accepted connection, socket %d\n", *new_socket);

        // Create thread to handle client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, new_socket) != 0) {
            perror("pthread_create failed");
            CLOSESOCKET(*new_socket);
            free(new_socket);
            continue;
        }

        pthread_detach(thread_id); // Auto cleanup thread resources when done
    }

    PQfinish(conn);
    CLOSESOCKET(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}
