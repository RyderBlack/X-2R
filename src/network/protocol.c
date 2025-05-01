#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "protocol.h"
#include "platform.h"

Message* create_message(MessageType type, const void* payload, uint32_t payload_size) {
    if (payload_size > MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "Payload size too large: %u > %u\n", payload_size, MAX_PAYLOAD_SIZE);
        return NULL;
    }
    
    Message* msg = malloc(sizeof(Message) + payload_size);
    if (!msg) {
        fprintf(stderr, "Failed to allocate memory for message\n");
        return NULL;
    }
    
    msg->type = type;
    msg->length = payload_size;
    
    if (payload && payload_size > 0) {
        memcpy(msg->payload, payload, payload_size);
    }
    
    return msg;
}

Message* create_auth_message(const char* username, const char* password) {
    if (!username || !password) {
        fprintf(stderr, "Username or password is NULL\n");
        return NULL;
    }
    
    AuthMessage auth = {0};
    strncpy(auth.username, username, sizeof(auth.username) - 1);
    strncpy(auth.password, password, sizeof(auth.password) - 1);
    
    return create_message(MSG_AUTH, &auth, sizeof(AuthMessage));
}

Message* create_chat_message(uint32_t channel_id, const char* content) {
    if (!content || channel_id == 0 || channel_id > INT32_MAX) {
        fprintf(stderr, "Invalid chat message parameters: channel_id=%u, content=%s\n", 
                channel_id, content ? content : "NULL");
        return NULL;
    }
    
    ChatMessage chat = {0};
    chat.channel_id = channel_id;
    strncpy(chat.content, content, sizeof(chat.content) - 1);
    
    return create_message(MSG_CHAT, &chat, sizeof(ChatMessage));
}

Message* create_join_channel_message(uint32_t channel_id) {
    if (channel_id == 0 || channel_id > INT32_MAX) {
        fprintf(stderr, "Invalid channel ID: %u\n", channel_id);
        return NULL;
    }
    
    return create_message(MSG_JOIN_CHANNEL, &channel_id, sizeof(uint32_t));
}

Message* create_leave_channel_message(uint32_t channel_id) {
    if (channel_id == 0 || channel_id > INT32_MAX) {
        fprintf(stderr, "Invalid channel ID: %u\n", channel_id);
        return NULL;
    }
    
    return create_message(MSG_LEAVE_CHANNEL, &channel_id, sizeof(uint32_t));
}

int send_message(SOCKET sock, const Message* msg) {
    if (sock == INVALID_SOCKET || !msg) {
        fprintf(stderr, "Invalid socket or message\n");
        return -1;
    }
    
    // Validate message type
    if (msg->type < MSG_AUTH || msg->type > MSG_ERROR) {
        fprintf(stderr, "Invalid message type: %d\n", msg->type);
        return -1;
    }
    
    // Validate payload size
    if (msg->length > MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "Payload size too large: %u\n", msg->length);
        return -1;
    }
    
    // Send header
    if (send(sock, (const char*)msg, sizeof(Message), 0) < 0) {
        perror("Failed to send message header");
        return -1;
    }
    
    // Send payload if exists
    if (msg->length > 0) {
        if (send(sock, msg->payload, msg->length, 0) < 0) {
            perror("Failed to send message payload");
            return -1;
        }
    }
    
    return 0;
}

Message* receive_message(SOCKET sock) {
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "Invalid socket\n");
        return NULL;
    }
    
    // Receive header
    Message header;
    int result = recv(sock, (char*)&header, sizeof(Message), 0);
    if (result <= 0) {
        if (result < 0) {
            perror("Failed to receive message header");
        }
        return NULL;
    }
    
    // Validate header
    if (header.type < MSG_AUTH || header.type > MSG_ERROR) {
        fprintf(stderr, "Received invalid message type: %d\n", header.type);
        return NULL;
    }
    
    if (header.length > MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "Received payload size too large: %u\n", header.length);
        return NULL;
    }
    
    // Allocate full message
    Message* msg = malloc(sizeof(Message) + header.length);
    if (!msg) {
        fprintf(stderr, "Failed to allocate memory for received message\n");
        return NULL;
    }
    
    // Copy header
    memcpy(msg, &header, sizeof(Message));
    
    // Receive payload if exists
    if (header.length > 0) {
        result = recv(sock, msg->payload, header.length, 0);
        if (result <= 0) {
            if (result < 0) {
                perror("Failed to receive message payload");
            }
            free(msg);
            return NULL;
        }
        
        // Validate received chat messages
        if (header.type == MSG_CHAT && header.length >= sizeof(ChatMessage)) {
            ChatMessage* chat = (ChatMessage*)msg->payload;
            if (chat->channel_id == 0 || chat->channel_id > INT32_MAX) {
                fprintf(stderr, "Received invalid channel ID: %u\n", chat->channel_id);
                free(msg);
                return NULL;
            }
        }
    }
    
    return msg;
}

Message* create_registration_message(const char *firstname, const char *lastname, const char *email, const char *password) {
    if (!firstname || !lastname || !email || !password) {
        fprintf(stderr, "Invalid registration data\n");
        return NULL;
    }

    RegistrationMessage reg = {0};
    strncpy(reg.firstname, firstname, sizeof(reg.firstname) - 1);
    strncpy(reg.lastname, lastname, sizeof(reg.lastname) - 1);
    strncpy(reg.email, email, sizeof(reg.email) - 1);
    strncpy(reg.password, password, sizeof(reg.password) - 1);

    return create_message(MSG_REGISTER, &reg, sizeof(RegistrationMessage));
}