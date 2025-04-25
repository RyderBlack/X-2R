#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"
#include "platform.h"

Message* create_message(MessageType type, const void* payload, uint32_t payload_size) {
    Message* msg = malloc(sizeof(Message) + payload_size);
    if (!msg) return NULL;
    
    msg->type = type;
    msg->length = payload_size;
    if (payload && payload_size > 0) {
        memcpy(msg->payload, payload, payload_size);
    }
    
    return msg;
}

Message* create_auth_message(const char* username, const char* password) {
    AuthMessage auth = {0};
    strncpy(auth.username, username, sizeof(auth.username) - 1);
    strncpy(auth.password, password, sizeof(auth.password) - 1);
    
    return create_message(MSG_AUTH, &auth, sizeof(AuthMessage));
}

Message* create_chat_message(uint32_t channel_id, const char* content) {
    ChatMessage chat = {0};
    chat.channel_id = channel_id;
    strncpy(chat.content, content, sizeof(chat.content) - 1);
    
    return create_message(MSG_CHAT, &chat, sizeof(ChatMessage));
}

Message* create_join_channel_message(uint32_t channel_id) {
    return create_message(MSG_JOIN_CHANNEL, &channel_id, sizeof(uint32_t));
}

Message* create_leave_channel_message(uint32_t channel_id) {
    return create_message(MSG_LEAVE_CHANNEL, &channel_id, sizeof(uint32_t));
}

int send_message(SOCKET sock, const Message* msg) {
    // Send header
    if (send(sock, (const char*)msg, sizeof(Message), 0) < 0) {
        return -1;
    }
    
    // Send payload if exists
    if (msg->length > 0) {
        if (send(sock, msg->payload, msg->length, 0) < 0) {
            return -1;
        }
    }
    
    return 0;
}

Message* receive_message(SOCKET sock) {
    // Receive header
    Message header;
    if (recv(sock, (char*)&header, sizeof(Message), 0) <= 0) {
        return NULL;
    }
    
    // Allocate full message
    Message* msg = malloc(sizeof(Message) + header.length);
    if (!msg) return NULL;
    
    // Copy header
    memcpy(msg, &header, sizeof(Message));
    
    // Receive payload if exists
    if (header.length > 0) {
        if (recv(sock, msg->payload, header.length, 0) <= 0) {
            free(msg);
            return NULL;
        }
    }
    
    return msg;
} 