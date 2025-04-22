// === Updated main.c with improved GTK GUI and CSS ===
#include <gtk/gtk.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db_connection.h"

#pragma comment(lib, "ws2_32.lib")

#define PORT 12345
#define MAX_MSG 1024

PGconn *global_conn;
GtkTextBuffer *text_buffer;

// === Server Code ===
void* client_handler(void* arg) {
    SOCKET client_socket = *(SOCKET*)arg;
    free(arg);
    char buffer[MAX_MSG] = {0};

    while (1) {
        int read_size = recv(client_socket, buffer, MAX_MSG - 1, 0);
        if (read_size <= 0) break;

        buffer[read_size] = '\0';
        printf("[Client] %s\n", buffer);

        const char *query = "INSERT INTO messages(content) VALUES($1)";
        const char *params[1] = { buffer };
        PGresult *res = PQexecParams(global_conn, query, 1, NULL, params, NULL, NULL, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            fprintf(stderr, "DB Error: %s\n", PQerrorMessage(global_conn));
        }
        PQclear(res);
    }

    closesocket(client_socket);
    return NULL;
}

int start_server() {
    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);
    printf("[Server] Listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        SOCKET *client_sock = malloc(sizeof(SOCKET));
        *client_sock = new_socket;
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, client_sock);
        pthread_detach(tid);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}

// === GTK Client UI ===
void send_message(GtkButton *button, gpointer entry) {
    const char *msg = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(msg) == 0) return;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
    };
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) >= 0) {
        send(sock, msg, strlen(msg), 0);
    }
    closesocket(sock);

    gtk_text_buffer_insert_at_cursor(text_buffer, "You: ", -1);
    gtk_text_buffer_insert_at_cursor(text_buffer, msg, -1);
    gtk_text_buffer_insert_at_cursor(text_buffer, "\n", -1);
    gtk_entry_set_text(GTK_ENTRY(entry), "");
}

void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background-color: #2f3136; color: white; }\n"
        "textview, entry { background-color: #40444b; color: white; border-radius: 6px; padding: 6px; }\n"
        "button { background-color: #5865f2; color: white; border-radius: 6px; padding: 6px; }\n"
        "button:hover { background-color: #4752c4; }\n",
        -1, NULL);

    GtkStyleContext *context = gtk_style_context_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void start_gui() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget *view = gtk_text_view_new();
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *entry = gtk_entry_new();
    GtkWidget *button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    g_signal_connect(button, "clicked", G_CALLBACK(send_message), entry);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    load_css();
    gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    global_conn = connect_to_db();
    if (!global_conn) return 1;

    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_CLOSE,
                                               "✅ Connected to the database successfully!");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    pthread_t server_thread;
    pthread_create(&server_thread, NULL, (void*(*)(void*))start_server, NULL);
    pthread_detach(server_thread);

    start_gui();
    gtk_main();

    PQfinish(global_conn);
    return 0;
}
