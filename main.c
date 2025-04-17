#include <gtk/gtk.h>
#include <libpq-fe.h>
#include "db_connection.h"

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);  // Initialize GTK

    PGconn *conn = connect_to_db();
    if (!conn) return 1;

    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_CLOSE,
                                               "✅ Connected to the database successfully!");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    PQfinish(conn);
    return 0;
}
