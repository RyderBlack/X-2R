#ifndef GTK_WRAPPER_H
#define GTK_WRAPPER_H

#include <gtk/gtk.h>
#include <pthread.h>
#include <libpq-fe.h>
#include "../network/platform.h"
#include "../network/protocol.h"

#define BUFFER_SIZE 1024

// Structure to hold all application widgets
typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *login_box;
    GtkWidget *register_box;
    GtkWidget *chat_box;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *login_button;
    GtkWidget *register_nav_button;
    GtkWidget *register_firstname_entry;
    GtkWidget *register_lastname_entry;
    GtkWidget *register_email_entry;
    GtkWidget *register_password_entry;
    GtkWidget *chat_input;
    GtkWidget *chat_history;
    GtkWidget *chat_channels_list;
    GtkWidget *contacts_list;
    GtkWidget *channel_name;
    SOCKET server_socket;
    pthread_t receive_thread;
    gboolean is_running;
    uint32_t current_channel_id;
    char username[256];
    PGconn *db_conn;
} AppWidgets;

// Structure for chat update data
typedef struct {
    AppWidgets *widgets;
    char sender[128];
    char content[BUFFER_SIZE];
    uint32_t channel_id;
} ChatUpdateData;

// Function declarations
extern void show_error_dialog(GtkWidget *parent, const char *message);
extern void load_channel_history(AppWidgets *widgets, uint32_t channel_id);
extern Message* create_registration_message(const char *firstname, const char *lastname, const char *email, const char *password);

// === initialize application ===
void activate_login(GtkApplication *app, gpointer user_data);
void activate_home(GtkApplication *app, gpointer user_data);

// === Utils ===
GtkWidget* create_login_window(GtkApplication *app);
GtkWidget* create_home_window(GtkApplication *app);

// === CSS Styling ===
void load_css(void);

#endif
