#include <gtk/gtk.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <libpq-fe.h>
#include <stdbool.h>
#include "platform.h"
#include "env_loader.h"
#include "protocol.h"
#include "db_connection.h"

#define BUFFER_SIZE 1024

// Structure pour stocker les widgets principaux
typedef struct
{
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *login_box;
    GtkWidget *chat_box;
    GtkWidget *register_box;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *register_firstname_entry;
    GtkWidget *register_lastname_entry;
    GtkWidget *register_email_entry;
    GtkWidget *register_password_entry;
    GtkWidget *chat_input;
    GtkWidget *chat_history;
    GtkWidget *chat_channels_list;
    GtkWidget *contacts_list;
    SOCKET server_socket;
    pthread_t receive_thread;
    gboolean is_running;
    uint32_t current_channel_id;
    char username[32];
    PGconn *db_conn;
} AppWidgets;

// Function to show error dialog
static void show_error_dialog(GtkWidget *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             "%s", message);
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

// Function to safely update the UI from the receive thread
static gboolean update_chat_history(gpointer data) {
    ChatMessage *msg = (ChatMessage *)data;
    AppWidgets *widgets = (AppWidgets *)g_object_get_data(G_OBJECT(widgets->window), "widgets");
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->chat_history));
    GtkTextIter end;
    
    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    // Format message with timestamp
    char formatted_msg[BUFFER_SIZE];
    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s: %s\n", 
             time_str, msg->sender_username, msg->content);
    
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, formatted_msg, -1);
    
    free(msg);
    return G_SOURCE_REMOVE;
}

// Thread function to receive messages
static void* receive_messages(void* arg) {
    AppWidgets *widgets = (AppWidgets *)arg;
    
    while (widgets->is_running) {
        Message* msg = receive_message(widgets->server_socket);
        if (!msg) {
            if (widgets->is_running) { // Only show error if not shutting down
                g_idle_add((GSourceFunc)show_error_dialog, widgets->window);
            }
            break;
        }
        
        switch (msg->type) {
            case MSG_CHAT: {
                ChatMessage* chat_msg = malloc(sizeof(ChatMessage));
                memcpy(chat_msg, msg->payload, sizeof(ChatMessage));
                g_idle_add((GSourceFunc)update_chat_history, chat_msg);
                break;
            }
            case MSG_USER_LIST: {
                UserInfo* users = malloc(sizeof(UserInfo) * 10); // Max 10 users
                memcpy(users, msg->payload, sizeof(UserInfo) * 10);
                g_idle_add((GSourceFunc)update_user_list, users);
                break;
            }
            case MSG_CHANNEL_LIST: {
                ChannelInfo* channels = malloc(sizeof(ChannelInfo) * 10); // Max 10 channels
                memcpy(channels, msg->payload, sizeof(ChannelInfo) * 10);
                g_idle_add((GSourceFunc)update_channel_list, channels);
                break;
            }
            case MSG_ERROR: {
                ErrorCode* error = (ErrorCode*)msg->payload;
                char error_msg[256];
                switch (*error) {
                    case ERR_AUTH_FAILED:
                        strcpy(error_msg, "Authentication failed");
                        break;
                    case ERR_INVALID_CHANNEL:
                        strcpy(error_msg, "Invalid channel");
                        break;
                    case ERR_NOT_AUTHORIZED:
                        strcpy(error_msg, "Not authorized");
                        break;
                    default:
                        strcpy(error_msg, "Server error");
                }
                g_idle_add((GSourceFunc)show_error_dialog, error_msg);
                break;
            }
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

// Function to handle login
static void on_login_button_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(widgets->username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(widgets->password_entry));
    
    if (strlen(username) > 0 && strlen(password) > 0) {
        // Check credentials in database
        const char *query = "SELECT user_id FROM users WHERE email = $1 AND password = $2";
        const char *params[2] = {username, password};
        PGresult *res = PQexecParams(widgets->db_conn, query, 2, NULL, params, NULL, NULL, 0);
        
        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
            printf("✅ Login successful for user: %s\n", username);
            
            // Update user status to online
            const char *update_query = "UPDATE users SET status = 'online' WHERE email = $1";
            const char *update_params[1] = {username};
            PGresult *update_res = PQexecParams(widgets->db_conn, update_query, 1, NULL, update_params, NULL, NULL, 0);
            PQclear(update_res);
            
            // Store username
            strncpy(widgets->username, username, sizeof(widgets->username) - 1);
            
            // Switch to chat view
            gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "chat");
        } else {
            printf("❌ Login failed for user: %s\n", username);
            show_error_dialog(widgets->window, "Invalid username or password");
        }
        PQclear(res);
    }
}

// Function to handle channel selection
static void on_channel_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    if (!row) return;
    
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
    const gchar *channel_name = gtk_label_get_text(GTK_LABEL(label));
    
    // Extract channel ID from the label (assuming format "#channel_name")
    uint32_t channel_id = atoi(channel_name + 1); // Skip the '#'
    
    if (channel_id != widgets->current_channel_id) {
        // Leave current channel
        if (widgets->current_channel_id != 0) {
            Message* leave_msg = create_leave_channel_message(widgets->current_channel_id);
            send_message(widgets->server_socket, leave_msg);
            free(leave_msg);
        }
        
        // Join new channel
        Message* join_msg = create_join_channel_message(channel_id);
        if (send_message(widgets->server_socket, join_msg) < 0) {
            show_error_dialog(widgets->window, "Failed to join channel");
        }
        free(join_msg);
        
        widgets->current_channel_id = channel_id;
        
        // Update chat input placeholder
        char placeholder[64];
        snprintf(placeholder, sizeof(placeholder), "Envoyer un message à %s", channel_name);
        gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->chat_input), placeholder);
    }
}

// Function to handle message sending
static void on_send_message(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *message = gtk_entry_get_text(GTK_ENTRY(widgets->chat_input));
    
    if (strlen(message) > 0 && widgets->current_channel_id != 0) {
        // Store message in database
        const char *query = "INSERT INTO messages (channel_id, sender_id, content) VALUES ($1, (SELECT user_id FROM users WHERE email = $2), $3)";
        const char *params[3] = {widgets->username, message};
        char channel_id_str[32];
        snprintf(channel_id_str, sizeof(channel_id_str), "%u", widgets->current_channel_id);
        params[0] = channel_id_str;
        
        PGresult *res = PQexecParams(widgets->db_conn, query, 3, NULL, params, NULL, NULL, 0);
        if (PQresultStatus(res) == PGRES_COMMAND_OK) {
            printf("📤 Message sent to channel %u: %s\n", widgets->current_channel_id, message);
        } else {
            printf("❌ Failed to store message in database\n");
        }
        PQclear(res);
        
        // Send message to server
        Message* chat_msg = create_chat_message(widgets->current_channel_id, message);
        if (send_message(widgets->server_socket, chat_msg) < 0) {
            show_error_dialog(widgets->window, "Failed to send message");
        }
        free(chat_msg);
        
        gtk_entry_set_text(GTK_ENTRY(widgets->chat_input), "");
    }
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
        
        // Insert new user
        const char *insert_query = "INSERT INTO users (first_name, last_name, email, password, status) VALUES ($1, $2, $3, $4, 'offline')";
        const char *insert_params[4] = {firstname, lastname, email, password};
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

// Fonction pour créer la page de connexion
static GtkWidget *create_login_page(AppWidgets *widgets)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // Titre
    GtkWidget *title = gtk_label_new("Connexion");
    gtk_widget_set_name(title, "login-title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    // Champ nom d'utilisateur
    widgets->username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->username_entry), "Nom d'utilisateur");
    gtk_box_pack_start(GTK_BOX(box), widgets->username_entry, FALSE, FALSE, 0);

    // Champ mot de passe
    widgets->password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->password_entry), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(widgets->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), widgets->password_entry, FALSE, FALSE, 0);

    // Bouton de connexion
    GtkWidget *login_button = gtk_button_new_with_label("Se connecter");
    gtk_widget_set_name(login_button, "login-button");
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), login_button, FALSE, FALSE, 10);

    // Bouton d'inscription
    GtkWidget *register_nav_button = gtk_button_new_with_label("S'inscrire");
    gtk_widget_set_name(register_nav_button, "register-nav-button");
    g_signal_connect(register_nav_button, "clicked", G_CALLBACK(on_register_nav_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), register_nav_button, FALSE, FALSE, 0);

    return box;
}

// Fonction pour créer la page d'inscription
static GtkWidget *create_register_page(AppWidgets *widgets)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // Titre
    GtkWidget *title = gtk_label_new("Inscription");
    gtk_widget_set_name(title, "register-title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    // Champ prénom
    widgets->register_firstname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_firstname_entry), "Prénom");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_firstname_entry, FALSE, FALSE, 0);

    // Champ nom
    widgets->register_lastname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_lastname_entry), "Nom");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_lastname_entry, FALSE, FALSE, 0);

    // Champ email
    widgets->register_email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_email_entry), "Email");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_email_entry, FALSE, FALSE, 0);

    // Champ mot de passe
    widgets->register_password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_password_entry), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(widgets->register_password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), widgets->register_password_entry, FALSE, FALSE, 0);

    // Bouton d'inscription
    GtkWidget *register_button = gtk_button_new_with_label("S'inscrire");
    gtk_widget_set_name(register_button, "register-button");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), register_button, FALSE, FALSE, 10);

    // Bouton de retour
    GtkWidget *back_button = gtk_button_new_with_label("Retour");
    gtk_widget_set_name(back_button, "back-button");
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), back_button, FALSE, FALSE, 0);

    return box;
}

// Fonction pour créer la page de discussion (comme Discord)
static GtkWidget *create_chat_page(AppWidgets *widgets)
{
    // Container principal
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // 1. Liste des canaux (à gauche)
    GtkWidget *channels_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(channels_box, 200, -1);
    gtk_widget_set_name(channels_box, "channels-box");

    GtkWidget *channels_title = gtk_label_new("Discussions");
    gtk_widget_set_name(channels_title, "channels-title");
    gtk_box_pack_start(GTK_BOX(channels_box), channels_title, FALSE, FALSE, 10);

    widgets->chat_channels_list = gtk_list_box_new();
    gtk_widget_set_name(widgets->chat_channels_list, "channels-list");

    // Ajout de quelques canaux d'exemple
    GtkWidget *channel1 = gtk_label_new("# général");
    gtk_widget_set_halign(channel1, GTK_ALIGN_START);
    gtk_widget_set_margin_start(channel1, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), channel1, -1);

    GtkWidget *channel2 = gtk_label_new("# aide");
    gtk_widget_set_halign(channel2, GTK_ALIGN_START);
    gtk_widget_set_margin_start(channel2, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), channel2, -1);

    GtkWidget *channel3 = gtk_label_new("# annonces");
    gtk_widget_set_halign(channel3, GTK_ALIGN_START);
    gtk_widget_set_margin_start(channel3, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), channel3, -1);

    GtkWidget *channels_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(channels_scroll), widgets->chat_channels_list);
    gtk_box_pack_start(GTK_BOX(channels_box), channels_scroll, TRUE, TRUE, 0);

    // 2. Zone centrale (historique des messages et zone de saisie)
    GtkWidget *chat_center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(chat_center_box, TRUE);

    // 2.1 Historique des messages
    widgets->chat_history = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(widgets->chat_history), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_name(widgets->chat_history, "chat-history");

    // Ajout de quelques messages d'exemple
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->chat_history));
    gtk_text_buffer_set_text(buffer, "Bienvenue dans #général\n\nUtilisateur1: Bonjour tout le monde!\n\nUtilisateur2: Salut! Comment ça va?\n\n", -1);

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
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->chat_input), "Envoyer un message à #général");
    gtk_widget_set_name(widgets->chat_input, "chat-input");
    gtk_widget_set_hexpand(widgets->chat_input, TRUE);

    GtkWidget *send_button = gtk_button_new_with_label("Envoyer");
    gtk_widget_set_name(send_button, "send-button");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_message), widgets);

    gtk_box_pack_start(GTK_BOX(input_box), widgets->chat_input, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_box), send_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(chat_center_box), input_box, FALSE, FALSE, 0);

    // 3. Liste des contacts (à droite)
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

    // Add channel selection handler
    g_signal_connect(widgets.chat_channels_list, "row-activated", 
                    G_CALLBACK(on_channel_selected), &widgets);

    gtk_widget_show_all(widgets.window);
    gtk_main();

    // Cleanup
    PQfinish(db_conn);
    CLEANUP_NETWORKING();

    return 0;
}