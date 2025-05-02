#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> // <-- NEW!
#include <stdbool.h> // <-- ADD THIS FOR bool, true, false
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
#include "security/encryption.h" // Include for decryption

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100 // Define max concurrent clients

// Structure to pass data to client handler thread
typedef struct {
    int socket;
    PGconn *db_conn;
    char authenticated_username[50]; // Store username after successful login
    uint32_t current_channel_id;     // Channel the client is currently viewing
} ClientData;

// --- Global Client List Management ---
ClientData* client_list[MAX_CLIENTS];
pthread_mutex_t client_list_mutex;

// Add a client to the global list
void add_client(ClientData* client) {
    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_list[i] == NULL) {
            client_list[i] = client;
            break;
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}

// Remove a client from the global list
void remove_client(ClientData* client) {
    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_list[i] == client) {
            client_list[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}

// Broadcast a message only to authenticated clients in the correct channel
void broadcast_message(Message* msg, int sender_socket) {
    // We need the payload to check the channel ID
    if (msg->type != MSG_CHAT || msg->length < sizeof(ChatMessage)) {
        fprintf(stderr, "Attempted to broadcast non-chat or invalid chat message.\n");
        return; 
    }
    ChatMessage* chat_payload = (ChatMessage*)msg->payload;
    uint32_t target_channel_id = chat_payload->channel_id;

    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_list[i] != NULL && 
            client_list[i]->socket != sender_socket && 
            client_list[i]->authenticated_username[0] != '\0' &&
            client_list[i]->current_channel_id == target_channel_id) // <-- Check channel ID!
        {
            if (send_message(client_list[i]->socket, msg) < 0) {
                perror("Broadcast send failed to socket"); 
                // Consider removing the client if send fails repeatedly
                // remove_client(client_list[i]); // Be careful with locking/iteration if you do this
            }
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}
// ------------------------------------

// Function to update user status (moved here or keep in a utils file? Let's put a simple version here for now)
static void update_user_db_status(PGconn *conn, const char *username, const char *status) {
    const char *update_query = "UPDATE users SET status = $1 WHERE email = $2";
    const char *params[2] = {status, username};
    PGresult *res = PQexecParams(conn, update_query, 2, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "DB Status Update Error for %s: %s\n", username, PQerrorMessage(conn));
    }
    PQclear(res);
}

// Thread function for handling a client
void* handle_client(void* arg) {
    ClientData *data = (ClientData *)arg;
    int client_socket = data->socket;
    PGconn *conn = data->db_conn;
    memset(data->authenticated_username, 0, sizeof(data->authenticated_username));
    data->current_channel_id = 0; // Initialize channel ID

    printf("📥 New thread started for client socket %d\n", client_socket);

    while (1) {
        Message* msg = receive_message(client_socket);
        if (!msg) {
            printf("❌ Client disconnected or error on socket %d (User: %s)\n", client_socket, data->authenticated_username[0] ? data->authenticated_username : "Unauthenticated");
            // If user was authenticated, update status to offline
            if (data->authenticated_username[0]) {
                update_user_db_status(conn, data->authenticated_username, "offline");
            }
            break;
        }

        printf("📝 Received message type: %d, length: %u from socket %d\n", msg->type, msg->length, client_socket);

        switch (msg->type) {
            case MSG_LOGIN_REQUEST: {
                // Prevent multiple login attempts on the same connection
                if (data->authenticated_username[0]) {
                    fprintf(stderr, "Warning: Already logged in user (%s) sent LOGIN_REQUEST on socket %d\n", data->authenticated_username, client_socket);
                    // Optionally send an error or just ignore
                    break;
                }

                LoginRequest *req = (LoginRequest*)msg->payload;
                printf("🔐 Login attempt for user: %s on socket %d\n", req->username, client_socket);

                // --- Database Authentication --- //
                const char *query = "SELECT user_id, password FROM users WHERE email = $1";
                const char *params[1] = {req->username};
                PGresult *res = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);

                bool login_ok = false;
                if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
                    const char *stored_password_encrypted = PQgetvalue(res, 0, 1);
                    // Basic check if password looks encrypted (adjust if needed)
                    int is_encrypted = 0;
                    for (int i = 0; stored_password_encrypted[i] != '\0'; i++) {
                        if (stored_password_encrypted[i] < 32 || stored_password_encrypted[i] > 126) {
                            is_encrypted = 1;
                            break;
                        }
                    }

                    if (is_encrypted) {
                        char decrypted_password[256];
                        size_t encrypted_len = strlen(stored_password_encrypted); // Use actual length if possible
                        xor_decrypt(stored_password_encrypted, decrypted_password, encrypted_len);
                         // Ensure null termination if decrypt doesn't guarantee it
                        decrypted_password[encrypted_len] = '\0'; // Be cautious here, might overwrite valid data if original wasn't simple XOR

                        if (strcmp(req->password, decrypted_password) == 0) {
                            login_ok = true;
                        }
                    } else {
                        // Handle plain text password comparison (legacy? should migrate)
                         if (strcmp(req->password, stored_password_encrypted) == 0) {
                            login_ok = true;
                        }
                    }
                }
                PQclear(res);
                // ----------------------------- //

                if (login_ok) {
                    printf("✅ Login successful for %s on socket %d\n", req->username, client_socket);
                    // Store authenticated username
                    strncpy(data->authenticated_username, req->username, sizeof(data->authenticated_username) - 1);
                    data->authenticated_username[sizeof(data->authenticated_username) - 1] = '\0'; // Ensure null termination

                    // Update DB status to online
                    update_user_db_status(conn, data->authenticated_username, "online");

                    // Add client to list *after* successful authentication
                    add_client(data); 

                    // Send success response
                    LoginSuccessResponse resp_payload;
                    strncpy(resp_payload.username, data->authenticated_username, sizeof(resp_payload.username) -1 );
                    resp_payload.username[sizeof(resp_payload.username)-1] = '\0';
                    Message *response = create_message(MSG_LOGIN_SUCCESS, &resp_payload, sizeof(LoginSuccessResponse));
                    if (response) {
                         send_message(client_socket, response);
                         free(response);
                    }
                } else {
                    printf("❌ Login failed for user: %s on socket %d\n", req->username, client_socket);
                    // Send failure response
                    Message *response = create_message(MSG_LOGIN_FAILURE, NULL, 0);
                     if (response) {
                         send_message(client_socket, response);
                         free(response);
                    }
                }
                break;
            }

            case MSG_REGISTER_REQUEST: {
                // Can only register if not already logged in
                if (data->authenticated_username[0]) {
                    fprintf(stderr, "Warning: Logged in user (%s) sent REGISTER_REQUEST on socket %d\n", data->authenticated_username, client_socket);
                    // Send failure? Or just ignore?
                    Message *response = create_message(MSG_REGISTER_FAILURE, NULL, 0); // Generic failure
                    if (response) {
                        send_message(client_socket, response);
                        free(response);
                    }
                    break;
                }

                RegisterRequest *req = (RegisterRequest*)msg->payload;
                printf("🔐 Registration attempt for email: %s\n", req->email);

                // --- Database Registration Logic --- //
                bool registration_ok = false;
                bool email_exists = false;

                // 1. Check if email exists
                const char *check_query = "SELECT 1 FROM users WHERE email = $1";
                const char *check_params[1] = {req->email};
                PGresult *check_res = PQexecParams(conn, check_query, 1, NULL, check_params, NULL, NULL, 0);
                if (PQresultStatus(check_res) == PGRES_TUPLES_OK) {
                    if (PQntuples(check_res) > 0) {
                        email_exists = true;
                    }
                } else {
                    fprintf(stderr, "DB Error checking email %s: %s\n", req->email, PQerrorMessage(conn));
                    // Send generic failure
                     Message *response = create_message(MSG_REGISTER_FAILURE, NULL, 0);
                    if (response) {
                        send_message(client_socket, response);
                        free(response);
                    }
                    PQclear(check_res);
                    break; // Exit case on DB error
                }
                 PQclear(check_res);

                // 2. If email doesn't exist, proceed with insertion
                if (!email_exists) {
                    // Encrypt password server-side
                    char encrypted_password[256]; // Ensure sufficient size
                    size_t password_len = strlen(req->password);
                     // Be careful with buffer sizes if password can be long
                    if (password_len >= sizeof(encrypted_password)) { 
                        fprintf(stderr, "Error: Password too long for encryption buffer.\n");
                        // Send generic failure
                        Message *response = create_message(MSG_REGISTER_FAILURE, NULL, 0);
                        if (response) {
                             send_message(client_socket, response);
                            free(response);
                        }
                        break; // Exit case
                    }
                    xor_encrypt(req->password, encrypted_password, password_len);
                    // IMPORTANT: xor_encrypt might not null-terminate if input fills buffer.
                    // If storing as text/varchar, it might be okay if length is managed.
                    // If storing as bytea, null termination isn't needed, but send length.
                    // Assuming VARCHAR storage for now.
                    encrypted_password[password_len] = '\0'; // Null-terminate for safety if needed by DB/later use

                    // Insert new user
                    const char *insert_query = "INSERT INTO users (first_name, last_name, email, password, status) VALUES ($1, $2, $3, $4, 'offline')";
                    // Use the encrypted password
                    const char *insert_params[4] = {req->firstname, req->lastname, req->email, encrypted_password};
                    PGresult *insert_res = PQexecParams(conn, insert_query, 4, NULL, insert_params, NULL, NULL, 0);

                    if (PQresultStatus(insert_res) == PGRES_COMMAND_OK) {
                        registration_ok = true;
                    } else {
                        fprintf(stderr, "DB Error inserting user %s: %s\n", req->email, PQerrorMessage(conn));
                    }
                    PQclear(insert_res);
                }
                // --------------------------------- //

                if (registration_ok) {
                    printf("✅ Registration successful for %s\n", req->email);
                    // Send success response
                    Message *response = create_message(MSG_REGISTER_SUCCESS, NULL, 0);
                    if (response) {
                        send_message(client_socket, response);
                        free(response);
                    }
                } else {
                    printf("❌ Registration failed for %s (Email exists: %s)\n", req->email, email_exists ? "Yes" : "No");
                    // Send failure response (could add payload with specific reason)
                    Message *response = create_message(MSG_REGISTER_FAILURE, NULL, 0); 
                     if (response) {
                        send_message(client_socket, response);
                        free(response);
                    }
                }
                break;
            }

            case MSG_JOIN_CHANNEL: { // Handle channel joining
                if (!data->authenticated_username[0]) {
                     fprintf(stderr, "Warning: Unauthenticated user tried to join channel.\n");
                     break;
                }
                if (msg->length >= sizeof(uint32_t)) {
                    uint32_t requested_channel_id = *((uint32_t*)msg->payload);
                    // TODO: Add server-side validation: Does channel exist? Does user have permission?
                    data->current_channel_id = requested_channel_id;
                    printf("👤 User %s (socket %d) joined channel %u\n", data->authenticated_username, client_socket, data->current_channel_id);
                } else {
                    fprintf(stderr, "Warning: Received invalid MSG_JOIN_CHANNEL payload size from socket %d\n", client_socket);
                }
                break;
            }

            case MSG_CHAT: {
                 if (!data->authenticated_username[0]) {
                    fprintf(stderr, "Warning: Unauthenticated user tried to send chat message.\n");
                    break;
                 }
                 
                 ChatMessage* chat = (ChatMessage*)msg->payload;
                 // Ensure the message is for the channel the client *claims* to be in?
                 // Or just trust the client sent it to the right place initially?
                 // For now, we'll trust the payload's channel_id for broadcasting.
                 
                 strncpy(chat->sender_username, data->authenticated_username, sizeof(chat->sender_username) - 1);
                 chat->sender_username[sizeof(chat->sender_username) - 1] = '\0';

                 printf("💬 Broadcasting [Channel %u] %s: %s (from socket %d)\n", chat->channel_id, chat->sender_username, chat->content, client_socket);
                 broadcast_message(msg, client_socket);
                 break;
            }

            default: {
                printf("❓ Received unhandled message type %d from socket %d (User: %s)\n", msg->type, client_socket, data->authenticated_username[0] ? data->authenticated_username : "Unauthenticated");
                break;
            }
        }

        free(msg);
    }

    // Cleanup: Remove client from list and close socket
    remove_client(data); // Remove from global list
    CLOSESOCKET(client_socket);
    printf("🔒 Connection closed for socket %d (User: %s)\n", client_socket, data->authenticated_username[0] ? data->authenticated_username : "Previously Unauthenticated");
    free(data); // Free the ClientData struct itself
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // Initialize client list and mutex
    memset(client_list, 0, sizeof(client_list));
    if (pthread_mutex_init(&client_list_mutex, NULL) != 0) {
        perror("Mutex initialization failed");
        return EXIT_FAILURE;
    }

    // Load environment variables from the SERVER configuration file
    if (!load_env(".env.server")) { 
        fprintf(stderr, "Failed to load server configuration .env.server\n");
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
        // Initialize the username field before passing to thread
        memset(data->authenticated_username, 0, sizeof(data->authenticated_username));

        printf("🔗 Accepted connection, socket %d\n", data->socket);

        // Create thread to handle client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, data) != 0) {
            perror("pthread_create failed");
            CLOSESOCKET(data->socket);
            free(data);
            continue;
        }
        // Don't add client here, add inside the thread upon successful login
        //add_client(data); 
        pthread_detach(thread_id);
    }

    // Cleanup
    pthread_mutex_destroy(&client_list_mutex);
    PQfinish(conn);
    CLOSESOCKET(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}
