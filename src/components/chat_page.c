#include <gtk/gtk.h>
#include <string.h>
#include "../network/protocol.h"
#include "../utils/string_utils.h"
#include "../types/app_types.h"
#include "chat_page.h"
#include "../utils/chat_utils.h"
#include <stdlib.h>
#include <libpq-fe.h>

static void on_send_message(GtkButton *button, gpointer user_data) {
    ChatPage *page = (ChatPage *)user_data;
    const gchar *message = gtk_entry_get_text(GTK_ENTRY(page->chat_input));
    
    if (!message || strlen(message) == 0) return;
    
    if (page->app_widgets->current_channel_id == 0) {
        show_error_dialog(page->app_widgets->window, "Please select a channel first");
        return;
    }
    
    // Store message in database
    if (!store_message(page->app_widgets, message, page->app_widgets->current_channel_id)) {
        return;
    }
    
    // Update chat history immediately
    update_chat_history(page->app_widgets, page->app_widgets->username, message, NULL);
    
    // Create and send message to server
    ChatMessage chat_msg;
    memset(&chat_msg, 0, sizeof(ChatMessage));
    chat_msg.channel_id = page->app_widgets->current_channel_id;
    strncpy(chat_msg.sender_username, page->app_widgets->username, sizeof(chat_msg.sender_username) - 1);
    strncpy(chat_msg.content, message, sizeof(chat_msg.content) - 1);
    
    Message *msg = create_message(MSG_CHAT, &chat_msg, sizeof(ChatMessage));
    if (!msg) return;
    
    if (send_message(page->app_widgets->server_socket, msg) < 0) {
        show_error_dialog(page->app_widgets->window, "Failed to send message to server");
    }
    
    free(msg);
    gtk_entry_set_text(GTK_ENTRY(page->chat_input), "");
}

static void on_channel_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    ChatPage *page = (ChatPage *)user_data;
    if (!row) {
        page->app_widgets->current_channel_id = 0;
        return;
    }
    
    const char *channel_id_str = g_object_get_data(G_OBJECT(row), "channel_id");
    if (!channel_id_str) {
        page->app_widgets->current_channel_id = 0;
        return;
    }
    
    uint32_t new_channel_id = (uint32_t)atoi(channel_id_str);
    if (new_channel_id == page->app_widgets->current_channel_id) {
        return;
    }
    
    // Update current channel
    page->app_widgets->current_channel_id = new_channel_id;
    
    // Update channel name label
    char channel_id_str_verify[32];
    snprintf(channel_id_str_verify, sizeof(channel_id_str_verify), "%u", new_channel_id);
    const char *query = "SELECT name FROM channels WHERE channel_id = $1";
    const char *params[1] = {channel_id_str_verify};
    PGresult *res = PQexecParams(page->app_widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
    
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *channel_name = PQgetvalue(res, 0, 0);
        char label_text[128];
        snprintf(label_text, sizeof(label_text), "# %s", channel_name);
        gtk_label_set_text(GTK_LABEL(page->channel_name), label_text);
    }
    
    PQclear(res);
    
    // Load channel history
    load_channel_history(page->app_widgets, new_channel_id);
}

ChatPage* chat_page_new(AppWidgets *app_widgets) {
    ChatPage *page = malloc(sizeof(ChatPage));
    page->app_widgets = app_widgets;
    
    // Main container
    page->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    // 1. Channel list (left)
    GtkWidget *channels_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(channels_box, 200, -1);
    gtk_widget_set_name(channels_box, "channels-box");
    
    GtkWidget *channels_title = gtk_label_new("Channels");
    gtk_widget_set_name(channels_title, "channels-title");
    gtk_box_pack_start(GTK_BOX(channels_box), channels_title, FALSE, FALSE, 10);
    
    page->chat_channels_list = gtk_list_box_new();
    gtk_widget_set_name(page->chat_channels_list, "channels-list");
    g_signal_connect(page->chat_channels_list, "row-selected", G_CALLBACK(on_channel_selected), page);
    
    GtkWidget *channels_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(channels_scroll), page->chat_channels_list);
    gtk_box_pack_start(GTK_BOX(channels_box), channels_scroll, TRUE, TRUE, 0);
    
    // 2. Chat area (center)
    GtkWidget *chat_center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(chat_center_box, TRUE);
    
    // Channel name
    page->channel_name = gtk_label_new("# Select a channel");
    gtk_widget_set_name(page->channel_name, "channel-name");
    gtk_widget_set_halign(page->channel_name, GTK_ALIGN_START);
    gtk_widget_set_margin_start(page->channel_name, 10);
    gtk_box_pack_start(GTK_BOX(chat_center_box), page->channel_name, FALSE, FALSE, 10);
    
    // Chat history
    page->chat_history = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(page->chat_history), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(page->chat_history), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(page->chat_history), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_name(page->chat_history, "chat-history");
    
    GtkWidget *history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(history_scroll), page->chat_history);
    gtk_widget_set_vexpand(history_scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(chat_center_box), history_scroll, TRUE, TRUE, 0);
    
    // Message input
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(input_box, 10);
    gtk_widget_set_margin_end(input_box, 10);
    gtk_widget_set_margin_top(input_box, 10);
    gtk_widget_set_margin_bottom(input_box, 10);
    
    page->chat_input = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->chat_input), "Type a message...");
    gtk_widget_set_hexpand(page->chat_input, TRUE);
    
    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_widget_set_name(send_button, "send-button");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_message), page);
    
    gtk_box_pack_start(GTK_BOX(input_box), page->chat_input, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_box), send_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(chat_center_box), input_box, FALSE, FALSE, 0);
    
    // 3. Contacts list (right)
    GtkWidget *contacts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(contacts_box, 200, -1);
    gtk_widget_set_name(contacts_box, "contacts-box");
    
    GtkWidget *contacts_title = gtk_label_new("Contacts");
    gtk_widget_set_name(contacts_title, "contacts-title");
    gtk_box_pack_start(GTK_BOX(contacts_box), contacts_title, FALSE, FALSE, 10);
    
    page->contacts_list = gtk_list_box_new();
    gtk_widget_set_name(page->contacts_list, "contacts-list");
    
    GtkWidget *contacts_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(contacts_scroll), page->contacts_list);
    gtk_box_pack_start(GTK_BOX(contacts_box), contacts_scroll, TRUE, TRUE, 0);
    
    // Assemble all parts
    gtk_box_pack_start(GTK_BOX(page->container), channels_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page->container), chat_center_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page->container), contacts_box, FALSE, FALSE, 0);
    
    return page;
}

void chat_page_free(ChatPage *page) {
    if (page) {
        free(page);
    }
}

GtkWidget* chat_page_get_container(ChatPage *page) {
    return page->container;
} 