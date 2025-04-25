#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "platform.h"
#include "env_loader.h"
#include "db_connection.h"
#include "chat_window.h"

#ifdef _WIN32
WSADATA wsaData;
#endif

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int sock = 0;
GtkWidget *chat_box_global = NULL; // To store the chat box widget globally

// Function to update chat window from main thread
gboolean update_chat_with_message(gpointer data) {
    const gchar *message = (const gchar *)data;

    GtkWidget *msg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *label = gtk_label_new(NULL);
    gchar *markup = g_markup_printf_escaped("<b><span foreground='blue'>Server:</span></b> %s", message);
    gtk_label_set_markup(GTK_LABEL(label), markup);

    gtk_box_pack_start(GTK_BOX(msg_box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(chat_box_global), msg_box, FALSE, FALSE, 4);

    gtk_widget_show_all(chat_box_global);
    g_free(markup);
    g_free(data);
    return FALSE;
}

static void receive_message() {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("📥 Received from server: %s\n", buffer);

        // Pass message to main thread for UI update
        g_idle_add(update_chat_with_message, g_strdup(buffer));
    }
}

void send_message(GtkWidget *widget, gpointer entry) {
    const gchar *message = gtk_entry_get_text(GTK_ENTRY(entry));
    if (message && strlen(message) > 0) {
        send(sock, message, strlen(message), 0);
        gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
}

static gboolean connect_to_server() {
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return FALSE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        return FALSE;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return FALSE;
    }

    printf("✅ Connected to server on port %d\n", SERVER_PORT);
    return TRUE;
}

int main(int argc, char *argv[]) {
    GtkWidget *window, *entry, *send_button;

    gtk_init(&argc, &argv);

#ifdef _WIN32
    INIT_NETWORKING();
#endif

    if (!connect_to_server()) {
        fprintf(stderr, "Unable to connect to server.\n");
        return EXIT_FAILURE;
    }

    // Build simple client GUI
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    chat_box_global = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(box), chat_box_global, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type a message...");
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);

    send_button = gtk_button_new_with_label("Send");
    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message), entry);
    gtk_box_pack_start(GTK_BOX(box), send_button, FALSE, FALSE, 0);

    gtk_widget_show_all(window);

    // Start receiving thread
    g_thread_new("receive_thread", (GThreadFunc)receive_message, NULL);

    gtk_main();

#ifdef _WIN32
    CLEANUP_NETWORKING();
#endif
    CLOSE_SOCKET(sock);

    return 0;
}
