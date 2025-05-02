#include <stdbool.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <libpq-fe.h>
#include <glib.h>
#include <stdio.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif
#include "network/platform.h"
#include "config/env_loader.h"
#include "network/protocol.h"
#include "database/db_connection.h"
#include "security/encryption.h"
#include "types/app_types.h"
#include "components/login_page.h"
#include "components/register_page.h"
#include "components/chat_page.h"
#include "utils/chat_utils.h"
#include "utils/string_utils.h"

#define BUFFER_SIZE 1024

// Function declarations for functions defined in this file
static gboolean update_user_list(gpointer data);
static gboolean update_channel_list(gpointer data);
static bool is_valid_channel_id(AppWidgets *widgets, uint32_t channel_id);
void show_error_dialog(GtkWidget *parent, const char *message);
static void ensure_default_role(PGconn *db_conn);
static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data);
void* receive_messages(void *arg);
static gboolean show_login_error_idle(gpointer data);
static gboolean show_registration_success_idle(gpointer data);
static gboolean show_registration_failure_idle(gpointer data);

// Function to sanitize UTF-8 strings for Pango
// NOTE: Consider moving this to gtk_string_utils.c if not already there
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

// Function to handle window close
static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    widgets->is_running = FALSE;
    
    // Close the socket to wake up the received thread
    CLOSE_SOCKET(widgets->server_socket);
    
    // Wait for the received thread to finish
    pthread_join(widgets->receive_thread, NULL);
    
    return FALSE; // Allow the window to close
}

// Function to receive messages from the server
void* receive_messages(void *arg) {
    AppWidgets *widgets = (AppWidgets *)arg;
    while (widgets->is_running) {
        Message *msg = receive_message(widgets->server_socket);
        if (!msg) {
            // Server disconnected or error
            fprintf(stderr, "Connection lost to server.\n");
            // Optionally, show an error and switch to login page or exit
            // g_idle_add(... show_error_dialog_and_quit ... , widgets);
            widgets->is_running = FALSE; // Stop the loop
            break;
        }

        switch (msg->type) {
            case MSG_LOGIN_SUCCESS: {
                LoginSuccessResponse *resp = (LoginSuccessResponse*)msg->payload;
                printf("✅ Login successful for user: %s\n", resp->username);
                // Need to run the UI updates on the main GTK thread
                // Create a copy of the username to pass to the idle function
                char *username_copy = g_strdup(resp->username);
                g_idle_add((GSourceFunc)finalize_login_ui_setup, username_copy); // Pass username
                break;
            }
            case MSG_LOGIN_FAILURE: {
                printf("❌ Login failed.\n");
                // Show error dialog on the main thread
                g_idle_add((GSourceFunc)show_login_error_idle, widgets);
                break;
            }
            case MSG_REGISTER_SUCCESS: {
                printf("✅ Registration successful.\n");
                g_idle_add((GSourceFunc)show_registration_success_idle, widgets);
                break;
            }
            case MSG_REGISTER_FAILURE: {
                 printf("❌ Registration failed.\n");
                 // TODO: Server could send back a reason (e.g., email exists) in payload
                 g_idle_add((GSourceFunc)show_registration_failure_idle, widgets);
                 break;
            }
            case MSG_CHAT: {
                ChatMessage *chat_msg = (ChatMessage *)msg->payload;

                // Server now sets the correct sender, so no need to check against widgets->username
                // Just prepare data for UI update
                ChatUpdateData *update_data = malloc(sizeof(ChatUpdateData));
                if (!update_data) {
                    fprintf(stderr, "Failed to allocate memory for chat update data\n");
                    break; // Break switch case, msg will be freed below
                }
                update_data->widgets = widgets;
                strncpy(update_data->sender, chat_msg->sender_username, sizeof(update_data->sender) - 1);
                strncpy(update_data->content, chat_msg->content, sizeof(update_data->content) - 1);
                update_data->channel_id = chat_msg->channel_id;
                update_data->sender[sizeof(update_data->sender) - 1] = '\0';
                update_data->content[sizeof(update_data->content) - 1] = '\0';

                g_idle_add((GSourceFunc)update_chat_history_from_network, update_data);
                break; // Don't free msg here, let it be freed after switch
            }
            // Add cases for other message types like MSG_USER_LIST, MSG_CHANNEL_LIST etc.
            default: {
                 printf("❓ Received unhandled message type: %d\n", msg->type);
                 break;
            }
        }
        free(msg);
    }
    printf("Exiting receive thread.\n");
    return NULL;
}

// Helper function to show login error dialog from idle callback
static gboolean show_login_error_idle(gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    show_error_dialog(widgets->window, "Invalid username or password");
    return G_SOURCE_REMOVE; // Run only once
}

// Helper function for registration success
static gboolean show_registration_success_idle(gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    // Show a success message
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(widgets->window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK,
                                               "Registration successful! You can now log in.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    // Switch to the login page
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "login");
    
    // Clear registration form fields (optional but good practice)
    gtk_entry_set_text(GTK_ENTRY(widgets->register_firstname_entry), "");
    gtk_entry_set_text(GTK_ENTRY(widgets->register_lastname_entry), "");
    gtk_entry_set_text(GTK_ENTRY(widgets->register_email_entry), "");
    gtk_entry_set_text(GTK_ENTRY(widgets->register_password_entry), "");
    
    return G_SOURCE_REMOVE; // Run only once
}

// Helper function for registration failure
static gboolean show_registration_failure_idle(gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    // TODO: Use specific error from server if provided in payload
    show_error_dialog(widgets->window, "Registration failed. Email might already exist.");
    return G_SOURCE_REMOVE; // Run only once
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

// Main function
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    // Load the CLIENT configuration file
    if (!load_env(".env.client")) { 
        fprintf(stderr, "Failed to load client configuration .env.client\n");
        // Consider providing a default server IP or exiting more gracefully
        return 1; 
    }

    // Retrieve the server IP from environment variable
    const char *server_ip = getenv("SERVER_IP");
    if (!server_ip) {
        fprintf(stderr, "SERVER_IP environment variable not set!\n");
        return 1;
    }

    // Initialize networking
    INIT_NETWORKING();

    // Create socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Connect to server using the IP from the environment variable
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        CLOSE_SOCKET(sock);
        return 1;
    }

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

    // Initialize AppWidgets
    AppWidgets app_widgets = {0};  // Zero initialize
    app_widgets.server_socket = sock;
    app_widgets.is_running = TRUE;
    app_widgets.current_channel_id = 0;
    app_widgets.db_conn = db_conn;

    // Create the window
    app_widgets.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (!gtk_window_set_icon_from_file(GTK_WINDOW(app_widgets.window), "icon.png", NULL)) {
        g_warning("Failed to load icon.");
    }
    gtk_window_set_title(GTK_WINDOW(app_widgets.window), "X-2R");
    gtk_window_set_default_size(GTK_WINDOW(app_widgets.window), 1920, 1080);
    g_object_set_data(G_OBJECT(app_widgets.window), "widgets", &app_widgets);

    // Connect window signals
    g_signal_connect(app_widgets.window, "delete-event", G_CALLBACK(on_window_delete), &app_widgets);
    g_signal_connect(app_widgets.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create stack
    app_widgets.stack = gtk_stack_new();
    gtk_container_add(GTK_CONTAINER(app_widgets.window), app_widgets.stack);

    // Create pages
    LoginPage *login_page = login_page_new(&app_widgets);
    RegisterPage *register_page = register_page_new(&app_widgets);
    ChatPage *chat_page = chat_page_new(&app_widgets);

    // Store widget pointers from pages into the global AppWidgets struct
    app_widgets.username_entry = login_page->username_entry;
    app_widgets.password_entry = login_page->password_entry;

    app_widgets.register_firstname_entry = register_page->firstname_entry;
    app_widgets.register_lastname_entry = register_page->lastname_entry;
    app_widgets.register_email_entry = register_page->email_entry;
    app_widgets.register_password_entry = register_page->password_entry;

    app_widgets.chat_input = chat_page->chat_input;
    app_widgets.chat_history = chat_page->chat_history;
    app_widgets.chat_channels_list = chat_page->chat_channels_list;
    app_widgets.contacts_list = chat_page->contacts_list;
    app_widgets.channel_name = chat_page->channel_name;

    // Add pages to stack
    gtk_stack_add_named(GTK_STACK(app_widgets.stack),
                       login_page_get_container(login_page), "login");
    gtk_stack_add_named(GTK_STACK(app_widgets.stack),
                       register_page_get_container(register_page), "register");
    gtk_stack_add_named(GTK_STACK(app_widgets.stack),
                       chat_page_get_container(chat_page), "chat");

    // Set default page
    gtk_stack_set_visible_child_name(GTK_STACK(app_widgets.stack), "login");

    // Load CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;

    const gchar *css_path = g_getenv("GTK_CSS_PATH");
    if (!css_path) {
        g_warning("GTK_CSS_PATH environment variable not set");
        // Correct fallback path relative to the build directory
        css_path = "C:\\Users\\ryrym_i6sf5hg\\CLionProjects\\X-RR\\src\\styles\\app.css";
    }

    if (!gtk_css_provider_load_from_path(provider, css_path, &error)) {
        g_warning("Failed to load CSS file '%s': %s", css_path, error->message);
        g_error_free(error);
    }

    gtk_style_context_add_provider_for_screen(
    gdk_screen_get_default(),
    GTK_STYLE_PROVIDER(provider),
    GTK_STYLE_PROVIDER_PRIORITY_USER
    );

    // Start receive thread
    if (pthread_create(&app_widgets.receive_thread, NULL, receive_messages, &app_widgets) != 0) {
        perror("Failed to create receive thread");
        CLOSE_SOCKET(sock);
        return 1;
    }

    gtk_widget_show_all(app_widgets.window);
    gtk_main();

    // Cleanup
    login_page_free(login_page);
    register_page_free(register_page);
    chat_page_free(chat_page);
    PQfinish(db_conn);
    CLEANUP_NETWORKING();

    return 0;
}