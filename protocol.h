#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"

// Define maximum payload size
#define MAX_PAYLOAD_SIZE 4096

// Make sure INVALID_SOCKET is defined
#ifndef INVALID_SOCKET
#ifdef _WIN32
#define INVALID_SOCKET INVALID_SOCKET
#else
#define INVALID_SOCKET (-1)
#endif
#endif

// Message types
typedef enum {
    MSG_AUTH,
    MSG_CHAT,
    MSG_USER_LIST,
    MSG_CHANNEL_LIST,
    MSG_JOIN_CHANNEL,
    MSG_LEAVE_CHANNEL,
    MSG_ERROR
} MessageType;

// Error codes
typedef enum {
    ERR_AUTH_FAILED,
    ERR_INVALID_CHANNEL,
    ERR_NOT_AUTHORIZED
} ErrorCode;

// User status
typedef enum {
    STATUS_ONLINE,
    STATUS_OFFLINE,
    STATUS_AWAY
} UserStatus;

// Structures
typedef struct {
    char username[32];
    char password[64];
} AuthMessage;

typedef struct {
    uint32_t channel_id;
    char sender_username[32];
    char content[1024];
} ChatMessage;

typedef struct {
    char username[32];
    UserStatus status;
} UserInfo;

typedef struct {
    uint32_t id;
    char name[64];
    bool is_private;
} ChannelInfo;

typedef struct {
    MessageType type;
    uint32_t length;
    char payload[];  // Flexible array member
} Message;

// Function prototypes
Message* create_message(MessageType type, const void* payload, uint32_t payload_size);
Message* create_auth_message(const char* username, const char* password);
Message* create_chat_message(uint32_t channel_id, const char* content);
Message* create_join_channel_message(uint32_t channel_id);
Message* create_leave_channel_message(uint32_t channel_id);
int send_message(SOCKET sock, const Message* msg);
Message* receive_message(SOCKET sock);

#endif // PROTOCOL_H 