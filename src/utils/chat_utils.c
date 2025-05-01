#include "chat_utils.h"
#include <stdbool.h>
#include <string.h>
#include <libpq-fe.h>
#include <time.h>
#include "../utils/gtk_string_utils.h" // For sanitize_utf8

bool store_message(AppWidgets *widgets, const char *message, uint32_t channel_id) {
    if (!widgets || !message || channel_id == 0) {
        printf("❌ Invalid parameters for store_message\n");
        return false;
    }

    printf("💾 Storing message for channel ID: %u\n", channel_id);
    
    // Get user_id for the current user
    const char *get_user_query = "SELECT user_id FROM users WHERE email = $1";
    const char *get_user_params[1] = {widgets->username};
    PGresult *user_res = PQexecParams(widgets->db_conn, get_user_query, 1, NULL, get_user_params, NULL, NULL, 0);
    
    if (PQresultStatus(user_res) != PGRES_TUPLES_OK || PQntuples(user_res) == 0) {
        printf("❌ Failed to get user information\n");
        PQclear(user_res);
        return false;
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

// Function to fetch the display name ("First Last") from DB
void get_display_name(AppWidgets *widgets, const char *sender_email, char *display_name, size_t size) {
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

// Helper function to add a message row to the list box and scroll
static void add_message_to_list_box(GtkListBox *list_box, const char *markup) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0); // Align text to the left
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE); // Enable line wrapping
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_margin_start(label, 10); // Add some left margin
    gtk_widget_set_margin_end(label, 10);   // Add some right margin
    gtk_widget_set_margin_top(label, 5);    // Add some top margin
    gtk_widget_set_margin_bottom(label, 5); // Add some bottom margin

    GtkWidget *row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(row), label);
    gtk_widget_set_name(row, "chat-message-row"); // Add CSS class for styling
    gtk_list_box_insert(list_box, row, -1); // Add to the end
    gtk_widget_show_all(row);

    // Auto-scroll logic for GtkListBox in GtkScrolledWindow
    GtkWidget *scrolled_window = gtk_widget_get_ancestor(GTK_WIDGET(list_box), GTK_TYPE_SCROLLED_WINDOW);
    if (scrolled_window) {
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
        // Scroll only if the user is near the bottom or hasn't scrolled up manually
        // A simple approach is to always scroll for now
        // TODO: Add logic to check if user scrolled up
        gtk_adjustment_set_value(vadj, gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));
    }
}

void update_chat_history(AppWidgets *widgets, const char *sender, const char *message, const char *channel_name) {
    if (!widgets || !widgets->chat_history || !message) {
        printf("❌ Invalid parameters for update_chat_history\n");
        return;
    }

    GtkListBox *list_box = GTK_LIST_BOX(widgets->chat_history);

    char display_name[128];
    get_display_name(widgets, sender, display_name, sizeof(display_name));

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    printf("📝 Updating chat history (ListBox): [%s] %s: %s\n", time_str, display_name, message);

    const char *safe_display_name_raw = sanitize_utf8(display_name);
    const char *safe_message = sanitize_utf8(message);

    char *escaped_display_name = g_markup_escape_text(safe_display_name_raw, -1);
    char *escaped_message = g_markup_escape_text(safe_message, -1);
    if (!escaped_display_name) escaped_display_name = g_strdup("");
    if (!escaped_message) escaped_message = g_strdup("");

    // Construct Pango markup string (similar format, maybe adjust later)
    char markup[BUFFER_SIZE + 256];
    snprintf(markup, sizeof(markup),
             "<b><span foreground='#786ee1' size='large'>%s</span></b> <span foreground='grey' size='small'>%s</span>\n%s",
             escaped_display_name, time_str, escaped_message);

    add_message_to_list_box(list_box, markup);

    g_free(escaped_display_name);
    g_free(escaped_message);

    /* Hyperlink detection logic would need to be adapted or removed if using simple labels */
}

gboolean update_chat_history_from_network(gpointer data) {
    ChatUpdateData *update_data = (ChatUpdateData *)data;
    if (!update_data || !update_data->widgets) {
        free(update_data);
        return G_SOURCE_REMOVE;
    }

    update_chat_history(update_data->widgets, 
                       update_data->sender, 
                       update_data->content, 
                       NULL);

    free(update_data);
    return G_SOURCE_REMOVE;
}

// Function to load channel history
void load_channel_history(AppWidgets *widgets, uint32_t channel_id) {
    if (!widgets || channel_id == 0) {
        printf("❌ Invalid parameters for load_channel_history\n");
        return;
    }

    GtkListBox *list_box = GTK_LIST_BOX(widgets->chat_history);

    // Clear existing messages
    GList *children = gtk_container_get_children(GTK_CONTAINER(list_box));
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    char channel_id_str[32];
    snprintf(channel_id_str, sizeof(channel_id_str), "%u", channel_id);

    const char *query = "SELECT u.email, m.content, m.timestamp FROM messages m "
                       "JOIN users u ON m.sender_id = u.user_id "
                       "WHERE m.channel_id = $1 ORDER BY m.timestamp ASC";
    const char *params[1] = {channel_id_str};

    PGresult *res = PQexecParams(widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);

        for (int i = 0; i < rows; i++) {
            const char *sender_email = PQgetvalue(res, i, 0);
            const char *content = PQgetvalue(res, i, 1);
            const char *db_timestamp = PQgetvalue(res, i, 2);

            char formatted_time[32];
            format_timestamp(db_timestamp, formatted_time, sizeof(formatted_time));

            char display_name[128];
            get_display_name(widgets, sender_email, display_name, sizeof(display_name));

            const char *safe_display_name_raw_loop = sanitize_utf8(display_name);
            const char *safe_content = sanitize_utf8(content);

            char *escaped_display_name_loop = g_markup_escape_text(safe_display_name_raw_loop, -1);
            char *escaped_content = g_markup_escape_text(safe_content, -1);
            if (!escaped_display_name_loop) escaped_display_name_loop = g_strdup("");
            if (!escaped_content) escaped_content = g_strdup("");

            char markup[BUFFER_SIZE + 256];
            snprintf(markup, sizeof(markup),
                     "<b><span foreground='#786ee1' size='large'>%s</span></b> <span foreground='grey' size='small'>%s</span>\n%s",
                     escaped_display_name_loop, formatted_time, escaped_content);

            // Add message as a new row
            add_message_to_list_box(list_box, markup);

            g_free(escaped_display_name_loop);
            g_free(escaped_content);
        }
        // Auto-scroll to the bottom after loading all messages
        // The add_message_to_list_box function handles scrolling incrementally,
        // but a final scroll ensures we are at the very bottom.
        GtkWidget *scrolled_window = gtk_widget_get_ancestor(GTK_WIDGET(list_box), GTK_TYPE_SCROLLED_WINDOW);
        if (scrolled_window) {
            GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
            gtk_adjustment_set_value(vadj, gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));
        }
    } else {
        printf("❌ Failed to load channel history: %s\n", PQerrorMessage(widgets->db_conn));
        // Optionally add an error message row to the list box
        add_message_to_list_box(list_box, "<span foreground='red'>Error loading history.</span>");
    }
    PQclear(res);
}

// Function to ensure user has proper channel associations
void ensure_user_channel_associations(AppWidgets *widgets, const char *user_id) {
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

// Function to refresh the channel list
void refresh_channel_list(AppWidgets *widgets) {

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

void handle_successful_login(AppWidgets *widgets, const char *username) {
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

// Function to format timestamp
void format_timestamp(const char *db_timestamp, char *formatted_time, size_t size) {
    int year, month, day, hour, minute, second;
    // Use sscanf_s on Windows if available and preferred for security
    #ifdef _WIN32
        sscanf_s(db_timestamp, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    #else
        sscanf(db_timestamp, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    #endif
    snprintf(formatted_time, size, "%02d:%02d:%02d", hour, minute, second);
} 