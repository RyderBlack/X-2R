#include <stdbool.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <libpq-fe.h>
#include <glib.h>
#include "network/platform.h"
#include "config/env_loader.h"
#include "network/protocol.h"
#include "database/db_connection.h"
#include "security/encryption.h"
#include "types/app_types.h"

#define BUFFER_SIZE 1024

// Function to sanitize UTF-8 strings for Pango
static const char* sanitize_utf8(const char* input) {
    static char sanitized[BUFFER_SIZE];
    
    if (!input) {
        sanitized[0] = '\0';
        return sanitized;
    }
    
    // Check if the string is valid UTF-8
    if (!g_utf8_validate(input, -1, NULL)) {
        // If invalid, copy character by character, replacing invalid sequences
        size_t i = 0, j = 0;
        while (input[i] != '\0' && j < BUFFER_SIZE - 1) {
            if ((input[i] & 0x80) == 0) {
                // ASCII character
                sanitized[j++] = input[i++];
            } else if ((input[i] & 0xE0) == 0xC0 && (input[i+1] & 0xC0) == 0x80) {
                // 2-byte UTF-8 sequence
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
            } else if ((input[i] & 0xF0) == 0xE0 && (input[i+1] & 0xC0) == 0x80 && (input[i+2] & 0xC0) == 0x80) {
                // 3-byte UTF-8 sequence
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
            } else if ((input[i] & 0xF8) == 0xF0 && (input[i+1] & 0xC0) == 0x80 && (input[i+2] & 0xC0) == 0x80 && (input[i+3] & 0xC0) == 0x80) {
                // 4-byte UTF-8 sequence
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
                sanitized[j++] = input[i++];
            } else {
                // Invalid sequence, replace with '?'
                sanitized[j++] = '?';
                i++;
            }
        }
        sanitized[j] = '\0';
    } else {
        // If valid, simply copy the string
        strncpy(sanitized, input, BUFFER_SIZE - 1);
        sanitized[BUFFER_SIZE - 1] = '\0';
    }
    
    return sanitized;
}

// Helper to sanitize UTF-8 strings for GTK display
static const char* sanitize_utf8_gtk(const char *str) {
    if (!str || !g_utf8_validate(str, -1, NULL)) {
        return "[Invalid UTF-8]";
    }
    return str;
}

// Function declarations

static gboolean update_user_list(gpointer data);
static gboolean update_channel_list(gpointer data);
static void format_timestamp(const char *db_timestamp, char *formatted_time, size_t size);
static void update_chat_history(AppWidgets *widgets, const char *sender, const char *message, const char *channel_name);
static bool store_message(AppWidgets *widgets, const char *message, uint32_t channel_id);
static void handle_successful_login(AppWidgets *widgets, const char *username);
static void refresh_channel_list(AppWidgets *widgets);
static void ensure_user_channel_associations(AppWidgets *widgets, const char *user_id);
static void ensure_default_role(PGconn *db_conn);

// Helper to check if a channel ID exists in the database
static bool is_valid_channel_id(AppWidgets *widgets, uint32_t channel_id) {
    char channel_id_str[32];
    snprintf(channel_id_str, sizeof(channel_id_str), "%u", channel_id);
    const char *query = "SELECT 1 FROM channels WHERE channel_id = $1";
    const char *params[1] = {channel_id_str};
    PGresult *res = PQexecParams(widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
    bool valid = (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    PQclear(res);
    return valid;
}

// Function to show error dialog
void show_error_dialog(GtkWidget *parent, const char *message) {
    // Sanitize message for UTF-8
    const char *safe_message = sanitize_utf8(message);
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", safe_message ? safe_message : "[No message]");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Function to update user list
static gboolean update_user_list(gpointer data) {
    UserInfo *users = (UserInfo *)data;
    AppWidgets *widgets = (AppWidgets *)g_object_get_data(G_OBJECT(widgets->window), "widgets");
    
    // Clear existing users
    GList *children = gtk_container_get_children(GTK_CONTAINER(widgets->contacts_list));
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    // Add new users
    for (int i = 0; i < 10; i++) { // Assuming max 10 users for now
        if (strlen(users[i].username) == 0) break;
        
        char user_text[64];
        snprintf(user_text, sizeof(user_text), "%s (%s)", users[i].username, 
                users[i].status == STATUS_ONLINE ? "online" : 
                users[i].status == STATUS_AWAY ? "away" : "offline");
        
        GtkWidget *user_label = gtk_label_new(user_text);
        gtk_widget_set_halign(user_label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(user_label, 10);
        gtk_list_box_insert(GTK_LIST_BOX(widgets->contacts_list), user_label, -1);
    }
    
    free(users);
    return G_SOURCE_REMOVE;
}

// Function to update channel list
static gboolean update_channel_list(gpointer data) {
    ChannelInfo *channels = (ChannelInfo *)data;
    AppWidgets *widgets = (AppWidgets *)g_object_get_data(G_OBJECT(widgets->window), "widgets");
    
    // Clear existing channels
    GList *children = gtk_container_get_children(GTK_CONTAINER(widgets->chat_channels_list));
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    // Add new channels
    for (int i = 0; i < 10; i++) { // Assuming max 10 channels for now
        if (strlen(channels[i].name) == 0) break;
        
        char channel_text[64];
        snprintf(channel_text, sizeof(channel_text), "#%s%s", 
                channels[i].name,
                channels[i].is_private ? " (private)" : "");
        
        GtkWidget *channel_label = gtk_label_new(channel_text);
        gtk_widget_set_halign(channel_label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(channel_label, 10);
        gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), channel_label, -1);
    }
    
    free(channels);
    return G_SOURCE_REMOVE;
}

// Function to format timestamp
static void format_timestamp(const char *db_timestamp, char *formatted_time, size_t size) {
    int year, month, day, hour, minute, second;
    sscanf(db_timestamp, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    snprintf(formatted_time, size, "%02d:%02d:%02d", hour, minute, second);
}

// Function to fetch the display name ("First Last") from DB
static void get_display_name(AppWidgets *widgets, const char *sender_email, char *display_name, size_t size) {
    if (!widgets || !sender_email || !display_name || size == 0)
        return;

    const char *query = "SELECT first_name, last_name FROM users WHERE email = $1";
    const char *params[1] = {sender_email};

    PGresult *res = PQexecParams(widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *first_name = PQgetvalue(res, 0, 0);
        const char *last_name = PQgetvalue(res, 0, 1);
        snprintf(display_name, size, "%s %s", first_name, last_name);
    } else {
        strncpy(display_name, sender_email, size - 1);
        display_name[size - 1] = '\0';
    }
    PQclear(res);
}

// Function to load channel history
void load_channel_history(AppWidgets *widgets, uint32_t channel_id) {
    if (!widgets || channel_id == 0) {
        printf("❌ Invalid parameters for load_channel_history\n");
        return;
    }

    char channel_id_str[32];
    snprintf(channel_id_str, sizeof(channel_id_str), "%u", channel_id);
    
    const char *query = "SELECT u.email, m.content, m.timestamp FROM messages m "
                       "JOIN users u ON m.sender_id = u.user_id "
                       "WHERE m.channel_id = $1 ORDER BY m.timestamp ASC";
    const char *params[1] = {channel_id_str};
    
    PGresult *res = PQexecParams(widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->chat_history));
        gtk_text_buffer_set_text(buffer, "", -1); // Start with empty chat history
        
        for (int i = 0; i < rows; i++) {
            const char *sender_email = PQgetvalue(res, i, 0);
            const char *content = PQgetvalue(res, i, 1);
            const char *db_timestamp = PQgetvalue(res, i, 2);
            
            // Format timestamp
            char formatted_time[32];
            format_timestamp(db_timestamp, formatted_time, sizeof(formatted_time));
            
            // Get sender's display name
            char display_name[128];
            get_display_name(widgets, sender_email, display_name, sizeof(display_name));
            
            // Format the message
            char formatted_msg[BUFFER_SIZE];
            snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s: %s\n", 
                     formatted_time, display_name, content);
            
            // Add to chat history
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, formatted_msg, -1);
        }
        
        // Scroll to the end
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(buffer, &end);
        GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, "end", &end, FALSE);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(widgets->chat_history), mark, 0.0, FALSE, 0.0, 0.0);
        gtk_text_buffer_delete_mark(buffer, mark);
    }
    PQclear(res);
}

// Function to update chat history with a new message
static void update_chat_history(AppWidgets *widgets, const char *sender, const char *message, const char *channel_name) {
    if (!widgets || !widgets->chat_history || !message) {
        printf("❌ Invalid parameters for update_chat_history\n");
        return;
    }
    
    // Get the sender's display name from the database
    char display_name[128];
    get_display_name(widgets, sender, display_name, sizeof(display_name));
    
    // Sanitize strings to ensure valid UTF-8
    const char *safe_display_name = sanitize_utf8(display_name);
    const char *safe_message = sanitize_utf8(message);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->chat_history));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    // Build a formatted message with sanitized strings
    char formatted_msg[BUFFER_SIZE];
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s: %s\n", 
             time_str, safe_display_name, safe_message);
    
    // Add to chat history
    gtk_text_buffer_insert(buffer, &end, formatted_msg, -1);
    
    // Auto-scroll to the bottom
    GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, "end", &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(widgets->chat_history), mark, 0.0, FALSE, 0.0, 0.0);
    gtk_text_buffer_delete_mark(buffer, mark);
}

// Function to store a message in the database
static bool store_message(AppWidgets *widgets, const char *message, uint32_t channel_id) {
    if (!widgets || !message || channel_id == 0) {
        printf("❌ Invalid parameters for store_message\n");
        return 0;
    }

    printf("💾 Storing message for channel ID: %u\n", channel_id);
    
    // Get user_id for the current user
    const char *get_user_query = "SELECT user_id FROM users WHERE email = $1";
    const char *get_user_params[1] = {widgets->username};
    PGresult *user_res = PQexecParams(widgets->db_conn, get_user_query, 1, NULL, get_user_params, NULL, NULL, 0);
    
    if (PQresultStatus(user_res) != PGRES_TUPLES_OK || PQntuples(user_res) == 0) {
        printf("❌ Failed to get user information\n");
        PQclear(user_res);
        return 0;
    }

    const char *user_id_str = PQgetvalue(user_res, 0, 0);
    printf("👤 Found user ID: %s\n", user_id_str);
    
    // Store message with proper UTF-8 handling
    const char *insert_query = "INSERT INTO messages (channel_id, sender_id, content) VALUES ($1, $2, $3)";
    const char *insert_params[3];
    char channel_id_str[32];
    snprintf(channel_id_str, sizeof(channel_id_str), "%u", channel_id);
    insert_params[0] = channel_id_str;
    insert_params[1] = user_id_str;
    insert_params[2] = message;
    
    PGresult *insert_res = PQexecParams(widgets->db_conn, insert_query, 3, NULL, insert_params, NULL, NULL, 0);
    bool success = (PQresultStatus(insert_res) == PGRES_COMMAND_OK);
    
    if (!success) {
        printf("❌ Failed to store message in database: %s\n", PQerrorMessage(widgets->db_conn));
    } else {
        printf("✅ Message stored successfully\n");
    }
    
    PQclear(insert_res);
    PQclear(user_res);
    return success;
}

// Add this new function after the handle_successful_login function
static void refresh_channel_list(AppWidgets *widgets) {

    printf("🔄 Refreshing channel list\n");

    // Clear existing channels first
    GList *children = gtk_container_get_children(GTK_CONTAINER(widgets->chat_channels_list));
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // Get user ID first
    const char *get_user_query = "SELECT user_id FROM users WHERE email = $1";
    const char *get_user_params[1] = {widgets->username};
    PGresult *user_res = PQexecParams(widgets->db_conn, get_user_query, 1, NULL, get_user_params, NULL, NULL, 0);
    
    if (PQresultStatus(user_res) != PGRES_TUPLES_OK || PQntuples(user_res) == 0) {
        printf("❌ Failed to get user information for channel list refresh\n");
        GtkWidget *error_label = gtk_label_new("Error loading channels");
        gtk_widget_set_halign(error_label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(error_label, 10);
        gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), error_label, -1);
        PQclear(user_res);
        return;
    }
    
    const char *user_id_str = PQgetvalue(user_res, 0, 0);
    printf("👤 Found user ID: %s for channel refresh\n", user_id_str);
    
    // Ensure user has channel associations
    ensure_user_channel_associations(widgets, user_id_str);
    
    // Get channels from database - two approaches:
    // 1. Get all channels visible to the user through user_channels
    const char *get_channels_query = 
        "SELECT DISTINCT c.channel_id, c.name FROM channels c "
        "LEFT JOIN user_channels uc ON c.channel_id = uc.channel_id "
        "WHERE (uc.user_id = $1 AND uc.channel_id IS NOT NULL) OR c.is_private = FALSE "
        "ORDER BY c.name";
    
    const char *params[1] = {user_id_str};
    PGresult *channels_res = PQexecParams(widgets->db_conn, get_channels_query, 1, NULL, params, NULL, NULL, 0);
    
    if (PQresultStatus(channels_res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(channels_res);
        if (rows == 0) {
            printf("⚠️ No channels found in database for user %s\n", widgets->username);
            // Create a label to show no channels
            GtkWidget *no_channels_label = gtk_label_new("No channels available");
            gtk_widget_set_halign(no_channels_label, GTK_ALIGN_START);
            gtk_widget_set_margin_start(no_channels_label, 10);
            gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), no_channels_label, -1);
        } else {
            printf("✅ Found %d channels for user in database\n", rows);
            for (int i = 0; i < rows; i++) {
                const char *channel_id_str = PQgetvalue(channels_res, i, 0);
                const char *channel_name = PQgetvalue(channels_res, i, 1);
                printf("📝 Adding channel: %s (ID: %s)\n", channel_name, channel_id_str);
                
                char label_text[64];
                snprintf(label_text, sizeof(label_text), "# %s", channel_name);
                
                GtkWidget *channel_label = gtk_label_new(label_text);
                gtk_widget_set_halign(channel_label, GTK_ALIGN_START);
                gtk_widget_set_margin_start(channel_label, 10);
                
                // Create row and store channel ID string and channel_name widget
                GtkWidget *row = gtk_list_box_row_new();
                gtk_container_add(GTK_CONTAINER(row), channel_label);
                char *channel_id_copy = g_strdup(channel_id_str);
                g_object_set_data_full(G_OBJECT(row), "channel_id", channel_id_copy, g_free);
                g_object_set_data(G_OBJECT(row), "channel_name", channel_label);
                gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), row, -1);
                
                // Select the general channel by default
                if (strcmp(channel_name, "general") == 0) {
                    gtk_list_box_select_row(GTK_LIST_BOX(widgets->chat_channels_list), GTK_LIST_BOX_ROW(row));
                    widgets->current_channel_id = (uint32_t)atoi(channel_id_str);
                }
            }
        }
    } else {
        printf("❌ Failed to query channels: %s\n", PQerrorMessage(widgets->db_conn));
        GtkWidget *error_label = gtk_label_new("Error loading channels");
        gtk_widget_set_halign(error_label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(error_label, 10);
        gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), error_label, -1);
    }
    
    PQclear(channels_res);
    PQclear(user_res);
    gtk_widget_show_all(widgets->chat_channels_list);
}

// Function to ensure user has proper channel associations
static void ensure_user_channel_associations(AppWidgets *widgets, const char *user_id) {
    printf("🔄 Ensuring user channel associations for user %s\n", widgets->username);
    
    // First, get all channels
    const char *get_channels_query = "SELECT channel_id FROM channels";
    PGresult *channels_res = PQexec(widgets->db_conn, get_channels_query);
    
    if (PQresultStatus(channels_res) != PGRES_TUPLES_OK) {
        printf("❌ Failed to query channels for association check: %s\n", PQerrorMessage(widgets->db_conn));
        PQclear(channels_res);
        return;
    }
    
    int num_channels = PQntuples(channels_res);
    if (num_channels == 0) {
        printf("⚠️ No channels found to associate with user\n");
        PQclear(channels_res);
        return;
    }
    
    printf("✅ Found %d channels to check for user associations\n", num_channels);
    
    // For each channel, check if user is associated, and if not, create the association
    for (int i = 0; i < num_channels; i++) {
        const char *channel_id = PQgetvalue(channels_res, i, 0);
        
        // Check if association exists
        const char *check_query = 
            "SELECT 1 FROM user_channels WHERE user_id = $1 AND channel_id = $2";
        const char *check_params[2] = {user_id, channel_id};
        PGresult *check_res = PQexecParams(widgets->db_conn, check_query, 2, NULL, check_params, NULL, NULL, 0);
        
        if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) == 0) {
            // Association doesn't exist, create it
            printf("➕ Creating association for user %s with channel %s\n", user_id, channel_id);
            
            // Default to role_id 1 (regular user)
            const char *insert_query = 
                "INSERT INTO user_channels (user_id, channel_id, role_id) VALUES ($1, $2, 1)";
            const char *insert_params[2] = {user_id, channel_id};
            PGresult *insert_res = PQexecParams(widgets->db_conn, insert_query, 2, NULL, insert_params, NULL, NULL, 0);
            
            if (PQresultStatus(insert_res) != PGRES_COMMAND_OK) {
                printf("❌ Failed to create user-channel association: %s\n", PQerrorMessage(widgets->db_conn));
            } else {
                printf("✅ Created user-channel association for channel %s\n", channel_id);
            }
            
            PQclear(insert_res);
        } else {
            printf("✓ User already associated with channel %s\n", channel_id);
        }
        
        PQclear(check_res);
    }
    
    PQclear(channels_res);
}

// Now update the handle_successful_login function to call refresh_channel_list
static void handle_successful_login(AppWidgets *widgets, const char *username) {
    // Update user status to online
    const char *update_query = "UPDATE users SET status = 'online' WHERE email = $1";
    const char *update_params[1] = {username};
    PGresult *update_res = PQexecParams(widgets->db_conn, update_query, 1, NULL, update_params, NULL, NULL, 0);
    PQclear(update_res);
    
    // Store username
    strncpy(widgets->username, username, sizeof(widgets->username) - 1);
    widgets->username[sizeof(widgets->username) - 1] = '\0';
    
    // Get user_id for the current user
    const char *get_user_query = "SELECT user_id FROM users WHERE email = $1";
    const char *get_user_params[1] = {username};
    PGresult *user_res = PQexecParams(widgets->db_conn, get_user_query, 1, NULL, get_user_params, NULL, NULL, 0);
    
    int channels_created = 0;
    
    if (PQresultStatus(user_res) == PGRES_TUPLES_OK && PQntuples(user_res) > 0) {
        const char *user_id_str = PQgetvalue(user_res, 0, 0);
        
        printf("👤 Found user ID %s for %s\n", user_id_str, username);
        
        // Create default channels if they don't exist
        const char *channels[] = {
            "general",
            "movies and tv shows",
            "memes",
            "music",
            "foodies"
        };
        
        for (int i = 0; i < 5; i++) {
            const char *check_channel_query = "SELECT channel_id FROM channels WHERE name = $1";
            const char *check_params[1] = {channels[i]};
            PGresult *check_res = PQexecParams(widgets->db_conn, check_channel_query, 1, NULL, check_params, NULL, NULL, 0);
            
            if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) == 0) {
                printf("➕ Creating new channel: %s\n", channels[i]);
                
                const char *create_channel_query = "INSERT INTO channels (name, is_private, created_by) VALUES ($1, FALSE, $2) RETURNING channel_id";
                const char *create_params[2] = {channels[i], user_id_str};
                PGresult *create_res = PQexecParams(widgets->db_conn, create_channel_query, 2, NULL, create_params, NULL, NULL, 0);
                
                if (PQresultStatus(create_res) == PGRES_TUPLES_OK && PQntuples(create_res) > 0) {
                    channels_created++;
                    printf("✅ Channel created with ID: %s\n", PQgetvalue(create_res, 0, 0));
                    
                    // Add user to the created channel with user_channels relation
                    const char *add_user_query = "INSERT INTO user_channels (user_id, channel_id, role_id) VALUES ($1, $2, 1)";
                    const char *add_user_params[2] = {user_id_str, PQgetvalue(create_res, 0, 0)};
                    PGresult *add_user_res = PQexecParams(widgets->db_conn, add_user_query, 2, NULL, add_user_params, NULL, NULL, 0);
                    
                    if (PQresultStatus(add_user_res) != PGRES_COMMAND_OK) {
                        printf("⚠️ Failed to add user to channel: %s\n", PQerrorMessage(widgets->db_conn));
                    }
                    
                    PQclear(add_user_res);
                } else {
                    printf("❌ Failed to create channel: %s\n", PQerrorMessage(widgets->db_conn));
                }
                
                PQclear(create_res);
            }
            PQclear(check_res);
        }
        
        // Get the general channel ID
        const char *get_general_query = "SELECT channel_id FROM channels WHERE name = 'general'";
        PGresult *general_res = PQexec(widgets->db_conn, get_general_query);
        
        if (PQresultStatus(general_res) == PGRES_TUPLES_OK && PQntuples(general_res) > 0) {
            widgets->current_channel_id = (uint32_t)atoi(PQgetvalue(general_res, 0, 0));
            printf("✅ Default channel ID set to: %u\n", widgets->current_channel_id);
        } else {
            printf("⚠️ Default channel not found\n");
        }
        PQclear(general_res);
    }
    PQclear(user_res);
    
    // Switch to chat view
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "chat");
    
    // Refresh the channel list
    refresh_channel_list(widgets);
    
    // Load channel history for general channel
    if (widgets->current_channel_id > 0) {
        load_channel_history(widgets, widgets->current_channel_id);
        
        // Update channel name label
        const char *get_name_query = "SELECT name FROM channels WHERE channel_id = $1";
        char channel_id_str[32];
        snprintf(channel_id_str, sizeof(channel_id_str), "%u", widgets->current_channel_id);
        const char *params[1] = {channel_id_str};
        PGresult *name_res = PQexecParams(widgets->db_conn, get_name_query, 1, NULL, params, NULL, NULL, 0);
        
        if (PQresultStatus(name_res) == PGRES_TUPLES_OK && PQntuples(name_res) > 0) {
            char label_text[64];
            snprintf(label_text, sizeof(label_text), "# %s", PQgetvalue(name_res, 0, 0));
            gtk_label_set_text(GTK_LABEL(widgets->channel_name), label_text);
        }
        
        PQclear(name_res);
    }
}

// Function to handle login
static void on_login_button_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(widgets->username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(widgets->password_entry));
    
    if (strlen(username) > 0 && strlen(password) > 0) {
        printf("🔍 Attempting login for user: %s\n", username);
        
        // Get user from database
        const char *query = "SELECT user_id, password FROM users WHERE email = $1";
        const char *params[1] = {username};
        PGresult *res = PQexecParams(widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
        
        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
            // Get stored password
            const char *stored_password = PQgetvalue(res, 0, 1);
            printf("🔑 Found user in database\n");
            printf("🔑 Stored password: %s\n", stored_password);
            
            // Check if the stored password is encrypted (contains non-printable characters)
            int is_encrypted = 0;
            for (int i = 0; stored_password[i] != '\0'; i++) {
                if (stored_password[i] < 32 || stored_password[i] > 126) {
                    is_encrypted = 1;
                    break;
                }
            }
            
            if (is_encrypted) {
                // Decrypt stored password
                char decrypted_password[256];
                xor_decrypt(stored_password, decrypted_password, strlen(stored_password));
                decrypted_password[strlen(stored_password)] = '\0';
                printf("🔑 Decrypted password: %s\n", decrypted_password);
                
                // Compare with input password
                if (strcmp(password, decrypted_password) == 0) {
                    printf("✅ Login successful (encrypted password)\n");
                    handle_successful_login(widgets, username);
                } else {
                    printf("❌ Login failed (encrypted password mismatch)\n");
                    show_error_dialog(widgets->window, "Invalid username or password");
                }
            } else {
                // Direct comparison for unencrypted passwords
                if (strcmp(password, stored_password) == 0) {
                    printf("✅ Login successful (direct comparison)\n");
                    handle_successful_login(widgets, username);
                } else {
                    printf("❌ Login failed (password mismatch)\n");
                    show_error_dialog(widgets->window, "Invalid username or password");
                }
            }
        } else {
            printf("❌ Login failed for user: %s (User not found)\n", username);
            show_error_dialog(widgets->window, "Invalid username or password");
        }
        PQclear(res);
    }
}

// Function to handle channel selection
static void on_channel_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    if (!row) {
        printf("No row selected. Resetting current_channel_id to 0.\n");
        widgets->current_channel_id = 0;
        return;
    }

    // Retrieve channel_id string stored in the row
    const char *row_channel_id_str = g_object_get_data(G_OBJECT(row), "channel_id");
    if (!row_channel_id_str) {
        printf("❌ No channel ID found in selected row. Resetting current_channel_id to 0.\n");
        widgets->current_channel_id = 0;
        return;
    }

    // Defensive: check if the string is a valid integer
    char *endptr = NULL;
    long channel_id_long = strtol(row_channel_id_str, &endptr, 10);
    if (endptr == row_channel_id_str || *endptr != '\0' || channel_id_long <= 0 || channel_id_long > INT32_MAX) {
        printf("❌ Channel ID is not a valid positive integer: %s. Resetting current_channel_id to 0.\n", row_channel_id_str);
        widgets->current_channel_id = 0;
        return;
    }

    uint32_t new_channel_id = (uint32_t)channel_id_long;
    printf("User selected channel row, channel_id: %u\n", new_channel_id);
    
    if (new_channel_id == widgets->current_channel_id) {
        printf("⚠️ Already in channel %u\n", new_channel_id);
        return;
    }

    // Verify channel exists in database
    if (!is_valid_channel_id(widgets, new_channel_id)) {
        printf("❌ Channel %u not found in database. Resetting current_channel_id to 0.\n", new_channel_id);
        widgets->current_channel_id = 0;
        return;
    }

    // Update current channel
    widgets->current_channel_id = new_channel_id;
    printf("current_channel_id set to: %u\n", widgets->current_channel_id);

    // Query the channel name for label update
    char channel_id_str_verify[32];
    snprintf(channel_id_str_verify, sizeof(channel_id_str_verify), "%u", new_channel_id);
    const char *query = "SELECT name FROM channels WHERE channel_id = $1";
    const char *params[1] = {channel_id_str_verify};
    PGresult *res = PQexecParams(widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
    
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *db_channel_name = PQgetvalue(res, 0, 0);
        if (db_channel_name && strlen(db_channel_name) > 0) {
            char label_buf[128];
            snprintf(label_buf, sizeof(label_buf), "# %s", db_channel_name);
            gtk_label_set_text(GTK_LABEL(widgets->channel_name), label_buf);
        }
    }
    
    PQclear(res);
    
    // Load channel history
    load_channel_history(widgets, new_channel_id);
    printf("✅ Switched to channel %u\n", new_channel_id);
}

// Function to send a message
static void on_send_message(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    if (!widgets || !widgets->chat_input) return;

    const gchar *message_text = gtk_entry_get_text(GTK_ENTRY(widgets->chat_input));
    if (!message_text || strlen(message_text) == 0) return;

    if (widgets->current_channel_id == 0) {
        show_error_dialog(widgets->window, "Invalid or deleted channel.");
        widgets->current_channel_id = 0;
        return;
    }

    if (!store_message(widgets, message_text, widgets->current_channel_id)) {
        return;
    }

    ChatMessage chat_msg;
    memset(&chat_msg, 0, sizeof(ChatMessage));
    chat_msg.channel_id = widgets->current_channel_id;

    strncpy(chat_msg.sender_username, widgets->username, sizeof(chat_msg.sender_username) - 1);
    chat_msg.sender_username[sizeof(chat_msg.sender_username) - 1] = '\0';
    printf("Sender username: %s\n", chat_msg.sender_username);


    strncpy(chat_msg.content, message_text, sizeof(chat_msg.content) - 1);
    chat_msg.content[sizeof(chat_msg.content) - 1] = '\0';

    Message *msg = create_message(MSG_CHAT, &chat_msg, sizeof(ChatMessage));
    if (!msg) return;

    if (send_message(widgets->server_socket, msg) < 0) {
        perror("send_message failed");
        show_error_dialog(widgets->window, "Failed to send message to server");
    }

    free(msg);
    gtk_entry_set_text(GTK_ENTRY(widgets->chat_input), "");
}

static gboolean update_chat_history_from_network(gpointer user_data) {
    ChatUpdateData *data = (ChatUpdateData *)user_data;
    if (!data) return FALSE;

    // Get the sender's display name from the database
    char display_name[128];
    get_display_name(data->widgets, data->sender, display_name, sizeof(display_name));
    
    // Sanitize strings to ensure valid UTF-8
    const char *safe_display_name = sanitize_utf8(display_name);
    const char *safe_message = sanitize_utf8(data->content);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->widgets->chat_history));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    // Build a formatted message with sanitized strings
    char formatted_msg[BUFFER_SIZE];
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s: %s\n", 
             time_str, safe_display_name, safe_message);
    
    // Add to chat history
    gtk_text_buffer_insert(buffer, &end, formatted_msg, -1);
    
    // Auto-scroll to the bottom
    GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, "end", &end, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(data->widgets->chat_history), mark, 0.0, FALSE, 0.0, 0.0);
    gtk_text_buffer_delete_mark(buffer, mark);

    free(data);
    return FALSE;
}

// Function to receive messages from the server
void* receive_messages(void *arg) {
    AppWidgets *widgets = (AppWidgets *)arg;
    while (widgets->is_running) {
        Message *msg = receive_message(widgets->server_socket);
        if (!msg) {
            break;
        }
        if (msg->type == MSG_CHAT) {
            ChatMessage *chat_msg = (ChatMessage *)msg->payload;

            // Get the sender's display name from the database
            char display_name[128];
            get_display_name(widgets, chat_msg->sender_username, display_name, sizeof(display_name));

            ChatUpdateData *update_data = malloc(sizeof(ChatUpdateData));
            if (!update_data) {
                free(msg);
                continue;
            }
            update_data->widgets = widgets;
            strncpy(update_data->sender, display_name, sizeof(update_data->sender) - 1);
            strncpy(update_data->content, chat_msg->content, sizeof(update_data->content) - 1);
            update_data->sender[sizeof(update_data->sender) - 1] = '\0';
            update_data->content[sizeof(update_data->content) - 1] = '\0';

            g_idle_add((GSourceFunc)update_chat_history_from_network, update_data);
        }
        free(msg);
    }
    return NULL;
}


// Function to handle window close
static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    widgets->is_running = FALSE;
    
    // Close the socket to wake up the receive thread
    CLOSE_SOCKET(widgets->server_socket);
    
    // Wait for the receive thread to finish
    pthread_join(widgets->receive_thread, NULL);
    
    return FALSE; // Allow the window to close
}

// Fonction de callback pour le bouton d'inscription sur la page login
static void on_register_nav_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "register");
}

// Function to handle registration
static void on_register_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *firstname = gtk_entry_get_text(GTK_ENTRY(widgets->register_firstname_entry));
    const gchar *lastname = gtk_entry_get_text(GTK_ENTRY(widgets->register_lastname_entry));
    const gchar *email = gtk_entry_get_text(GTK_ENTRY(widgets->register_email_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(widgets->register_password_entry));

    if (strlen(firstname) > 0 && strlen(lastname) > 0 && strlen(email) > 0 && strlen(password) > 0) {
        // Check if email already exists
        const char *check_query = "SELECT user_id FROM users WHERE email = $1";
        const char *check_params[1] = {email};
        PGresult *check_res = PQexecParams(widgets->db_conn, check_query, 1, NULL, check_params, NULL, NULL, 0);
        
        if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
            printf("❌ Registration failed - Email already exists: %s\n", email);
            show_error_dialog(widgets->window, "Email already exists");
            PQclear(check_res);
            return;
        }
        PQclear(check_res);
        
        // Encrypt password
        char encrypted_password[256];
        size_t password_len = strlen(password);
        xor_encrypt(password, encrypted_password, password_len);
        
        // Insert new user with encrypted password
        const char *insert_query = "INSERT INTO users (first_name, last_name, email, password, status) VALUES ($1, $2, $3, $4, 'offline')";
        const char *insert_params[4] = {firstname, lastname, email, encrypted_password};
        PGresult *insert_res = PQexecParams(widgets->db_conn, insert_query, 4, NULL, insert_params, NULL, NULL, 0);
        
        if (PQresultStatus(insert_res) == PGRES_COMMAND_OK) {
            printf("✅ Registration successful for user: %s\n", email);
            // Return to login page
            gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "login");
        } else {
            printf("❌ Registration failed for user: %s\n", email);
            show_error_dialog(widgets->window, "Registration failed");
        }
        PQclear(insert_res);
    }
}

// Fonction de callback pour le bouton de retour
static void on_back_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "login");
}

// Function to create the login page
static GtkWidget *create_login_page(AppWidgets *widgets)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // Title
    GtkWidget *title = gtk_label_new("Login");
    gtk_widget_set_name(title, "login-title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    // Username field
    widgets->username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->username_entry), "Username");
    gtk_box_pack_start(GTK_BOX(box), widgets->username_entry, FALSE, FALSE, 0);

    // Password field
    widgets->password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(widgets->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), widgets->password_entry, FALSE, FALSE, 0);

    // Login button
    GtkWidget *login_button = gtk_button_new_with_label("Login");
    gtk_widget_set_name(login_button, "login-button");
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), login_button, FALSE, FALSE, 10);

    // Register button
    GtkWidget *register_nav_button = gtk_button_new_with_label("Register");
    gtk_widget_set_name(register_nav_button, "register-nav-button");
    g_signal_connect(register_nav_button, "clicked", G_CALLBACK(on_register_nav_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), register_nav_button, FALSE, FALSE, 0);

    return box;
}

// Function to create the registration page
static GtkWidget *create_register_page(AppWidgets *widgets)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // Title
    GtkWidget *title = gtk_label_new("Registration");
    gtk_widget_set_name(title, "register-title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    // First name field
    widgets->register_firstname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_firstname_entry), "First name");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_firstname_entry, FALSE, FALSE, 0);

    // Last name field
    widgets->register_lastname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_lastname_entry), "Last name");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_lastname_entry, FALSE, FALSE, 0);

    // Email field
    widgets->register_email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_email_entry), "Email");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_email_entry, FALSE, FALSE, 0);

    // Password field
    widgets->register_password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(widgets->register_password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), widgets->register_password_entry, FALSE, FALSE, 0);

    // Register button
    GtkWidget *register_button = gtk_button_new_with_label("Register");
    gtk_widget_set_name(register_button, "register-button");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), register_button, FALSE, FALSE, 10);

    // Back button
    GtkWidget *back_button = gtk_button_new_with_label("Back");
    gtk_widget_set_name(back_button, "back-button");
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), back_button, FALSE, FALSE, 0);

    return box;
}

// Function to create the chat page
static GtkWidget *create_chat_page(AppWidgets *widgets) {
    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // 1. Channel list (on the left)
    GtkWidget *channels_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(channels_box, 200, -1);
    gtk_widget_set_name(channels_box, "channels-box");

    GtkWidget *channels_title = gtk_label_new("Channels");
    gtk_widget_set_name(channels_title, "channels-title");
    gtk_box_pack_start(GTK_BOX(channels_box), channels_title, FALSE, FALSE, 10);

    widgets->chat_channels_list = gtk_list_box_new();
    gtk_widget_set_name(widgets->chat_channels_list, "channels-list");

    // Create a placeholder instead of loading channels here
    // Channels will be loaded by refresh_channel_list after login
    GtkWidget *loading_label = gtk_label_new("Loading channels...");
    gtk_widget_set_halign(loading_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(loading_label, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), loading_label, -1);

    // Add channel selection handler (single click selection)
    g_signal_connect(widgets->chat_channels_list, "row-selected", 
                    G_CALLBACK(on_channel_selected), widgets);

    GtkWidget *channels_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(channels_scroll), widgets->chat_channels_list);
    gtk_box_pack_start(GTK_BOX(channels_box), channels_scroll, TRUE, TRUE, 0);

    // 2. Zone centrale (historique des messages et zone de saisie)
    GtkWidget *chat_center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(chat_center_box, TRUE);

    // Add channel name label at the top
    widgets->channel_name = gtk_label_new("# Select a channel");
    gtk_widget_set_name(widgets->channel_name, "channel-name");
    gtk_widget_set_halign(widgets->channel_name, GTK_ALIGN_START);
    gtk_widget_set_margin_start(widgets->channel_name, 10);
    gtk_widget_set_margin_top(widgets->channel_name, 10);
    gtk_widget_set_margin_bottom(widgets->channel_name, 10);
    gtk_box_pack_start(GTK_BOX(chat_center_box), widgets->channel_name, FALSE, FALSE, 0);

    // 2.1 Historique des messages
    widgets->chat_history = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(widgets->chat_history), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_name(widgets->chat_history, "chat-history");

    GtkWidget *history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(history_scroll), widgets->chat_history);
    gtk_widget_set_vexpand(history_scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(chat_center_box), history_scroll, TRUE, TRUE, 0);

    // 2.2 Zone de saisie
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(input_box, 10);
    gtk_widget_set_margin_end(input_box, 10);
    gtk_widget_set_margin_top(input_box, 10);
    gtk_widget_set_margin_bottom(input_box, 10);

    widgets->chat_input = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->chat_input), "Select a channel to start chatting");
    gtk_widget_set_name(widgets->chat_input, "chat-input");
    gtk_widget_set_hexpand(widgets->chat_input, TRUE);

    GtkWidget *send_button = gtk_button_new_with_label("Envoyer");
    gtk_widget_set_name(send_button, "send-button");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_message), widgets);

    gtk_box_pack_start(GTK_BOX(input_box), widgets->chat_input, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_box), send_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(chat_center_box), input_box, FALSE, FALSE, 0);

    // 3. Contact list (on the right)
    GtkWidget *contacts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(contacts_box, 200, -1);
    gtk_widget_set_name(contacts_box, "contacts-box");

    GtkWidget *contacts_title = gtk_label_new("Contacts");
    gtk_widget_set_name(contacts_title, "contacts-title");
    gtk_box_pack_start(GTK_BOX(contacts_box), contacts_title, FALSE, FALSE, 10);

    widgets->contacts_list = gtk_list_box_new();
    gtk_widget_set_name(widgets->contacts_list, "contacts-list");

    // Ajout de quelques contacts d'exemple
    GtkWidget *contact1 = gtk_label_new("Utilisateur1");
    gtk_widget_set_halign(contact1, GTK_ALIGN_START);
    gtk_widget_set_margin_start(contact1, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->contacts_list), contact1, -1);

    GtkWidget *contact2 = gtk_label_new("Utilisateur2");
    gtk_widget_set_halign(contact2, GTK_ALIGN_START);
    gtk_widget_set_margin_start(contact2, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->contacts_list), contact2, -1);

    GtkWidget *contact3 = gtk_label_new("Utilisateur3");
    gtk_widget_set_halign(contact3, GTK_ALIGN_START);
    gtk_widget_set_margin_start(contact3, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->contacts_list), contact3, -1);

    GtkWidget *contacts_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(contacts_scroll), widgets->contacts_list);
    gtk_box_pack_start(GTK_BOX(contacts_box), contacts_scroll, TRUE, TRUE, 0);

    // Assembler les trois parties
    gtk_box_pack_start(GTK_BOX(main_box), channels_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), chat_center_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), contacts_box, FALSE, FALSE, 0);

    return main_box;
}

// Function to ensure default role exists
static void ensure_default_role(PGconn *db_conn) {
    printf("🔄 Ensuring default role exists\n");
    
    // Check if any roles exist
    const char *check_query = "SELECT COUNT(*) FROM roles";
    PGresult *check_res = PQexec(db_conn, check_query);
    
    if (PQresultStatus(check_res) != PGRES_TUPLES_OK) {
        printf("❌ Failed to check roles: %s\n", PQerrorMessage(db_conn));
        PQclear(check_res);
        return;
    }
    
    int count = atoi(PQgetvalue(check_res, 0, 0));
    PQclear(check_res);
    
    if (count == 0) {
        printf("➕ Creating default role 'user'\n");
        
        // Insert default role
        const char *insert_query = "INSERT INTO roles (role_id, name) VALUES (1, 'user')";
        PGresult *insert_res = PQexec(db_conn, insert_query);
        
        if (PQresultStatus(insert_res) != PGRES_COMMAND_OK) {
            printf("❌ Failed to create default role: %s\n", PQerrorMessage(db_conn));
        } else {
            printf("✅ Created default role\n");
        }
        
        PQclear(insert_res);
    } else {
        printf("✅ Roles already exist in the database\n");
    }
}

// Fonction principale
int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    // Initialize networking
    INIT_NETWORKING();

    // Create socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Connect to server
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        CLOSE_SOCKET(sock);
        return 1;
    }

    // Connect to database
    PGconn *db_conn = connect_to_db();
    if (!db_conn) {
        fprintf(stderr, "Failed to connect to database\n");
        CLOSE_SOCKET(sock);
        return 1;
    }
    
    // Ensure default roles exist
    ensure_default_role(db_conn);

    // Création de la fenêtre principale
    AppWidgets widgets;
    widgets.server_socket = sock;
    widgets.is_running = TRUE;
    widgets.current_channel_id = 0;
    widgets.db_conn = db_conn;
    
    // Create the window first
    widgets.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(widgets.window), "Diskorde");
    gtk_window_set_default_size(GTK_WINDOW(widgets.window), 1000, 600);
    
    // Now that the window is created, we can store the widgets pointer
    g_object_set_data(G_OBJECT(widgets.window), "widgets", &widgets);
    
    // Connect the delete event to handle cleanup
    g_signal_connect(widgets.window, "delete-event", G_CALLBACK(on_window_delete), &widgets);
    g_signal_connect(widgets.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Création du stack pour gérer les pages
    widgets.stack = gtk_stack_new();
    gtk_container_add(GTK_CONTAINER(widgets.window), widgets.stack);

    // Création des pages
    widgets.login_box = create_login_page(&widgets);
    widgets.chat_box = create_chat_page(&widgets);
    widgets.register_box = create_register_page(&widgets);

    // Ajout des pages au stack
    gtk_stack_add_named(GTK_STACK(widgets.stack), widgets.login_box, "login");
    gtk_stack_add_named(GTK_STACK(widgets.stack), widgets.chat_box, "chat");
    gtk_stack_add_named(GTK_STACK(widgets.stack), widgets.register_box, "register");

    // Définition de la page de connexion comme page par défaut
    gtk_stack_set_visible_child_name(GTK_STACK(widgets.stack), "login");

    // Chargement du CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
                                    "window { background-color: black; }"
                                    "#login-title, #register-title { font-size: 24px; color: red; margin-bottom: 20px; }"
                                    "#channels-title, #contacts-title { font-size: 18px; color: red; font-weight: bold; }"
                                    "#channel-name { font-size: 20px; color: red; font-weight: bold; }"
                                    "entry { padding: 8px; border-radius: 4px; border: 1px solid red; color: white; background-color: black; margin: 5px 0; }"
                                    "entry:focus { border-color: #ff3333; }"
                                    "#login-button, #register-button, #register-nav-button, #back-button, #send-button { padding: 10px 20px; background-color: red; color: red; border: none; border-radius: 4px; }"
                                    "#login-button:hover, #register-button:hover, #register-nav-button:hover, #back-button:hover, #send-button:hover { background-color: #cc0000; }"
                                    "#chat-history { background-color: #111111; color: white; }"
                                    "#channels-box, #contacts-box { background-color: #1a1a1a; border-right: 1px solid #333333; border-left: 1px solid #333333; }"
                                    "#channels-list, #contacts-list { background-color: #1a1a1a; color: white; }"
                                    "#channels-list label, #contacts-list label { padding: 8px 0; }"
                                    "#channels-list label:hover, #contacts-list label:hover { background-color: #333333; }",
                                    -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Start the receive thread
    if (pthread_create(&widgets.receive_thread, NULL, receive_messages, &widgets) != 0) {
        perror("Failed to create receive thread");
        CLOSE_SOCKET(sock);
        return 1;
    }

    gtk_widget_show_all(widgets.window);
    gtk_main();

    // Cleanup
    PQfinish(db_conn);
    CLEANUP_NETWORKING();

    return 0;
}