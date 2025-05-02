#include <gtk/gtk.h>
#include <string.h>
#include "../network/protocol.h"
#include "../utils/string_utils.h"
#include "../types/app_types.h"
#include "chat_page.h"
#include "../utils/chat_utils.h"
#include <stdlib.h>
#include <libpq-fe.h>

// Forward declaration for the event handler
// static gboolean on_chat_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);

// Forward declaration for logout handler
static void on_logout_button_clicked(GtkButton *button, gpointer user_data);

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

// Callback function to handle events (like clicks) in chat history
// static gboolean on_chat_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
//     GtkTextView *text_view = GTK_TEXT_VIEW(widget);
//     GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

//     // Handle button release events to detect link clicks
//     if (event->type == GDK_BUTTON_RELEASE) {
//         GdkEventButton *button_event = (GdkEventButton *)event;
//         gint buffer_x, buffer_y;
//         GtkTextIter iter;

//         // Convert window coordinates to buffer coordinates
//         gtk_text_view_window_to_buffer_coords(text_view,
//                                               GTK_TEXT_WINDOW_WIDGET, // Use widget window
//                                               button_event->x,
//                                               button_event->y,
//                                               &buffer_x, &buffer_y);

//         // Get the iterator at the clicked location
//         gtk_text_view_get_iter_at_location(text_view, &iter, buffer_x, buffer_y);

//         // Check if the hyperlink tag is applied at this location
//         // Note: We need to check if the tag has the name "hyperlink"
//         // The previous check using g_object_get_data("tag-name") was incorrect.
//         // We should iterate tags and compare their names directly.
//         GSList *tags = gtk_text_iter_get_tags(&iter);
//         gboolean link_found = FALSE;
//         char *clicked_url = NULL;

//         for (GSList *l = tags; l != NULL; l = l->next) {
//             GtkTextTag *tag = GTK_TEXT_TAG(l->data);
//             const gchar* name = g_object_get_data(G_OBJECT(tag), "name"); // Get tag name
//              if (name && strcmp(name, "hyperlink") == 0) {
//                 // Found the hyperlink tag. Now get the text range it covers.
//                 GtkTextIter range_start, range_end;
//                 range_start = iter;
//                 range_end = iter;
//                 
//                 // Ensure we are inside the tag boundaries before getting text
//                 if (gtk_text_iter_has_tag(&iter, tag)) {
//                     // Move iterators to the boundaries of the tag toggle
//                     gtk_text_iter_backward_to_tag_toggle(&range_start, tag);
//                     gtk_text_iter_forward_to_tag_toggle(&range_end, tag);

//                     // Extract the text within the tagged range
//                     clicked_url = gtk_text_buffer_get_text(buffer, &range_start, &range_end, FALSE);
//                     link_found = TRUE;
//                     break; // Found the link
//                 }
//             }
//         }
//         g_slist_free(tags);

//         if (link_found && clicked_url) {
//             printf("🔗 Link clicked: %s\n", clicked_url);
//             GtkWidget *window = gtk_widget_get_toplevel(widget);
//             gtk_show_uri_on_window(GTK_WINDOW(window), clicked_url, GDK_CURRENT_TIME, NULL);
//             g_free(clicked_url);
//             return TRUE; // Event handled
//         }
//     }

//     // Return FALSE to allow default handling for other events
//     return FALSE;
// }

// Logout button callback
static void on_logout_button_clicked(GtkButton *button, gpointer user_data) {
    ChatPage *page = (ChatPage *)user_data;
    AppWidgets *widgets = page->app_widgets;

    // Update user status to offline
    update_user_status(widgets, widgets->username, "offline");

    // Switch back to the login page
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "login");

    // Optionally clear fields or reset state if needed
    gtk_entry_set_text(GTK_ENTRY(widgets->username_entry), ""); // Clear username on login page
    gtk_entry_set_text(GTK_ENTRY(widgets->password_entry), ""); // Clear password on login page
    widgets->current_channel_id = 0; // Reset current channel
    // Clear chat history display?
    GtkListBox *list_box = GTK_LIST_BOX(widgets->chat_history);
    GList *children = gtk_container_get_children(GTK_CONTAINER(list_box));
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    gtk_label_set_text(GTK_LABEL(widgets->channel_name), "# Select a channel"); // Reset channel name label
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

    // Add User display label above the logout button
    // Initialize with placeholder - will be updated on login
    page->app_widgets->user_display_label = gtk_label_new("Logged in as:"); 
    gtk_widget_set_name(page->app_widgets->user_display_label, "user-display-label");
    gtk_widget_set_halign(page->app_widgets->user_display_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(page->app_widgets->user_display_label, 10);
    gtk_widget_set_margin_end(page->app_widgets->user_display_label, 10);
    gtk_box_pack_end(GTK_BOX(channels_box), page->app_widgets->user_display_label, FALSE, FALSE, 5); // Pack before logout

    // Add Logout button at the bottom of the channels box
    page->logout_button = gtk_button_new_with_label("Logout");
    gtk_widget_set_name(page->logout_button, "logout-button");
    gtk_widget_set_margin_top(page->logout_button, 10); // Add some space above
    gtk_widget_set_margin_bottom(page->logout_button, 10); // Add some space below
    gtk_widget_set_margin_start(page->logout_button, 10);
    gtk_widget_set_margin_end(page->logout_button, 10);
    g_signal_connect(page->logout_button, "clicked", G_CALLBACK(on_logout_button_clicked), page);
    gtk_box_pack_end(GTK_BOX(channels_box), page->logout_button, FALSE, FALSE, 0); // Pack at the end
    
    // 2. Chat area (center)
    GtkWidget *chat_center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(chat_center_box, TRUE);
    
    // Channel name
    page->channel_name = gtk_label_new("# Select a channel");
    gtk_widget_set_name(page->channel_name, "channel-name");
    gtk_widget_set_halign(page->channel_name, GTK_ALIGN_START);
    gtk_widget_set_margin_start(page->channel_name, 10);
    gtk_box_pack_start(GTK_BOX(chat_center_box), page->channel_name, FALSE, FALSE, 10);
    
    // Chat history - Use GtkListBox
    page->chat_history = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(page->chat_history), GTK_SELECTION_NONE); // Disable row selection
    gtk_widget_set_name(page->chat_history, "chat-history-list"); // New ID for CSS

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