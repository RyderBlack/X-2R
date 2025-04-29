#include <gtk/gtk.h>
#include <string.h>
#include "../network/protocol.h"
#include "../utils/string_utils.h"
#include "../types/app_types.h"
#include "chat_page.h"

static void on_send_message(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    
    const char *message = gtk_entry_get_text(GTK_ENTRY(widgets->chat_input));
    if (!message || strlen(message) == 0) {
        return;
    }
    
    if (widgets->current_channel_id == 0) {
        show_error_dialog(widgets->window, "Please select a channel first");
        return;
    }
    
    // Create and send chat message
    Message *chat_msg = create_chat_message(widgets->current_channel_id, message);
    if (!chat_msg) {
        show_error_dialog(widgets->window, "Failed to create chat message");
        return;
    }
    
    if (send_message(widgets->server_socket, chat_msg) < 0) {
        show_error_dialog(widgets->window, "Failed to send chat message");
        free(chat_msg);
        return;
    }
    
    // Clear input
    gtk_entry_set_text(GTK_ENTRY(widgets->chat_input), "");
    
    free(chat_msg);
}

static void on_channel_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    
    if (!row) return;
    
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
    const char *channel_name = gtk_label_get_text(GTK_LABEL(label));
    
    // Find channel ID from name
    uint32_t channel_id = 0;
    // TODO: Implement channel ID lookup
    
    if (channel_id == 0) {
        show_error_dialog(widgets->window, "Invalid channel selected");
        return;
    }
    
    widgets->current_channel_id = channel_id;
    gtk_label_set_text(GTK_LABEL(widgets->channel_name), channel_name);
    
    // Load channel history
    load_channel_history(widgets, channel_id);
}

GtkWidget* create_chat_page(AppWidgets *widgets) {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    // Left panel (channels)
    GtkWidget *channels_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(channels_box, 200, -1);
    gtk_widget_set_name(channels_box, "channels-box");
    
    GtkWidget *channels_label = gtk_label_new("Channels");
    gtk_widget_set_name(channels_label, "channels-title");
    gtk_box_pack_start(GTK_BOX(channels_box), channels_label, FALSE, FALSE, 5);
    
    widgets->chat_channels_list = gtk_list_box_new();
    gtk_widget_set_name(widgets->chat_channels_list, "channels-list");
    g_signal_connect(widgets->chat_channels_list, "row-selected", G_CALLBACK(on_channel_selected), widgets);
    gtk_box_pack_start(GTK_BOX(channels_box), widgets->chat_channels_list, TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), channels_box, FALSE, FALSE, 0);
    
    // Middle panel (chat)
    GtkWidget *chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    
    // Channel name
    widgets->channel_name = gtk_label_new("Select a channel");
    gtk_widget_set_name(widgets->channel_name, "channel-name");
    gtk_box_pack_start(GTK_BOX(chat_box), widgets->channel_name, FALSE, FALSE, 5);
    
    // Chat history
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    widgets->chat_history = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_widget_set_name(widgets->chat_history, "chat-history");
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), widgets->chat_history);
    gtk_box_pack_start(GTK_BOX(chat_box), scrolled_window, TRUE, TRUE, 0);
    
    // Message input
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    widgets->chat_input = gtk_entry_new();
    gtk_widget_set_name(widgets->chat_input, "chat-input");
    
    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_widget_set_name(send_button, "send-button");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_message), widgets);
    
    gtk_box_pack_start(GTK_BOX(input_box), widgets->chat_input, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_box), send_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(chat_box), input_box, FALSE, FALSE, 5);
    
    gtk_box_pack_start(GTK_BOX(main_box), chat_box, TRUE, TRUE, 0);
    
    // Right panel (contacts)
    GtkWidget *contacts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(contacts_box, 200, -1);
    gtk_widget_set_name(contacts_box, "contacts-box");
    
    GtkWidget *contacts_label = gtk_label_new("Contacts");
    gtk_widget_set_name(contacts_label, "contacts-title");
    gtk_box_pack_start(GTK_BOX(contacts_box), contacts_label, FALSE, FALSE, 5);
    
    widgets->contacts_list = gtk_list_box_new();
    gtk_widget_set_name(widgets->contacts_list, "contacts-list");
    gtk_box_pack_start(GTK_BOX(contacts_box), widgets->contacts_list, TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), contacts_box, FALSE, FALSE, 0);
    
    return main_box;
} 