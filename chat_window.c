#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *chat_box;
static GtkWidget *entry;
static void (*send_callback_func)(const gchar *, gpointer) = NULL;
static gpointer send_callback_data = NULL;

gboolean append_message_to_chat(gpointer data) {
    const gchar *message = (const gchar *)data;

    GtkWidget *msg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *label = gtk_label_new(NULL);
    gchar *markup = g_markup_printf_escaped("<b><span foreground='blue'>Server:</span></b> %s", message);
    gtk_label_set_markup(GTK_LABEL(label), markup);

    gtk_box_pack_start(GTK_BOX(msg_box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(chat_box), msg_box, FALSE, FALSE, 4);

    gtk_widget_show_all(chat_box);
    g_free(markup);
    g_free(data);
    return FALSE;
}

static void on_send_clicked(GtkWidget *widget, gpointer data) {
    const gchar *msg = gtk_entry_get_text(GTK_ENTRY(entry));
    if (msg && strlen(msg) > 0) {
        if (send_callback_func) {
            send_callback_func(msg, send_callback_data);
        }
        gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
}

void start_chat_window(void (*send_callback)(const gchar *, gpointer), gpointer userdata) {
    send_callback_func = send_callback;
    send_callback_data = userdata;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "X2R Chat");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(box), chat_box, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type a message...");
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);

    GtkWidget *send_btn = gtk_button_new_with_label("Send");
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box), send_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
}
