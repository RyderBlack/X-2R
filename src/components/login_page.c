#include <gtk/gtk.h>
#include "../types/app_types.h"
#include "login_page.h"
#include <string.h>
#include "../database/db_connection.h"
#include "../security/encryption.h"
#include "../network/protocol.h"
#include "../utils/string_utils.h"

// Function to create the login page
GtkWidget *create_login_page(AppWidgets *widgets) {
    // Create main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);

    // Create title label
    GtkWidget *title_label = gtk_label_new("Login");
    gtk_widget_set_name(title_label, "title-label");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    // Create username entry
    GtkWidget *username_label = gtk_label_new("Username:");
    GtkWidget *username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Username");
    gtk_widget_set_name(username_entry, "username-entry");
    gtk_box_pack_start(GTK_BOX(vbox), username_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), username_entry, FALSE, FALSE, 0);

    // Create password entry
    GtkWidget *password_label = gtk_label_new("Password:");
    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_widget_set_name(password_entry, "password-entry");
    gtk_box_pack_start(GTK_BOX(vbox), password_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), password_entry, FALSE, FALSE, 0);

    // Create login button
    GtkWidget *login_button = gtk_button_new_with_label("Login");
    gtk_widget_set_name(login_button, "login-button");
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(vbox), login_button, FALSE, FALSE, 5);

    // Create register navigation button
    GtkWidget *register_button = gtk_button_new_with_label("Don't have an account? Register");
    gtk_widget_set_name(register_button, "register-nav-button");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_nav_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(vbox), register_button, FALSE, FALSE, 5);

    // Store references in AppWidgets
    widgets->username_entry = username_entry;
    widgets->password_entry = password_entry;
    widgets->login_button = login_button;
    widgets->register_nav_button = register_button;

    return vbox;
}

void on_login_button_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    
    const char *username = gtk_entry_get_text(GTK_ENTRY(widgets->username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(widgets->password_entry));
    
    if (!username || !password || strlen(username) == 0 || strlen(password) == 0) {
        show_error_dialog(widgets->window, "Please enter both username and password");
        return;
    }
    
    // Create and send authentication message
    Message *auth_msg = create_auth_message(username, password);
    if (!auth_msg) {
        show_error_dialog(widgets->window, "Failed to create authentication message");
        return;
    }
    
    if (send_message(widgets->server_socket, auth_msg) < 0) {
        show_error_dialog(widgets->window, "Failed to send authentication message");
        free(auth_msg);
        return;
    }
    
    free(auth_msg);
}

void on_register_nav_button_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    // TODO: Implement navigation to registration page
    g_print("Navigate to registration page\n");
} 