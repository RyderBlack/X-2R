#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <gtk/gtk.h>
#include "env_loader.h"

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);  // Initialize GTK

    load_env_file(".env");

    const char *host = getenv("PG_HOST");
    const char *port = getenv("PG_PORT");
    const char *dbname = getenv("PG_DB");
    const char *user = getenv("PG_USER");
    const char *password = getenv("PG_PASSWORD");

    if (!host || !port || !dbname || !user || !password) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Missing environment variables!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return 1;
    }

    char conninfo[512];
    snprintf(conninfo, sizeof(conninfo),
             "host=%s port=%s dbname=%s user=%s password=%s",
             host, port, dbname, user, password);

    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Connection failed: %s", PQerrorMessage(conn));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        PQfinish(conn);
        return 1;
    }

    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_CLOSE,
                                               "✅ Connected to the database successfully!");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    PQfinish(conn);
    return 0;
}
