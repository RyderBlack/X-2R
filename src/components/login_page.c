#include <gtk/gtk.h>
#include "../types/app_types.h"
#include "login_page.h"
#include "../utils/chat_utils.h"
#include <string.h>
#include "../database/db_connection.h"
#include "../security/encryption.h"
#include "../network/protocol.h"
#include "../utils/string_utils.h"
#include <stdlib.h>
#include <libpq-fe.h>

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
    gtk_widget_set_name(register_button, "register-button");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_nav_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(vbox), register_button, FALSE, FALSE, 5);

    // Store references in AppWidgets
    widgets->username_entry = username_entry;
    widgets->password_entry = password_entry;
    widgets->login_button = login_button;
    widgets->register_nav_button = register_button;

    return vbox;
}

void on_login_button_clicked(GtkButton *button, gpointer user_data) {
    LoginPage *page = (LoginPage *)user_data;
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(page->username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(page->password_entry));
    
    // Print the username being used for login attempt
    printf("Attempting login with username: %s\n", username ? username : "NULL");
    
    if (strlen(username) > 0 && strlen(password) > 0) {
        // Get user from database
        const char *query = "SELECT user_id, password FROM users WHERE email = $1";
        const char *params[1] = {username};
        PGresult *res = PQexecParams(page->app_widgets->db_conn, query, 1, NULL, params, NULL, NULL, 0);
        
        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
            const char *stored_password = PQgetvalue(res, 0, 1);
            
            // Check if the stored password is encrypted
            int is_encrypted = 0;
            for (int i = 0; stored_password[i] != '\0'; i++) {
                if (stored_password[i] < 32 || stored_password[i] > 126) {
                    is_encrypted = 1;
                    break;
                }
            }
            
            if (is_encrypted) {
                char decrypted_password[256];
                xor_decrypt(stored_password, decrypted_password, strlen(stored_password));
                decrypted_password[strlen(stored_password)] = '\0';
                
                if (strcmp(password, decrypted_password) == 0) {
                    handle_successful_login(page->app_widgets, username);
                } else {
                    show_error_dialog(page->app_widgets->window, "Invalid username or password");
                }
            } else {
                if (strcmp(password, stored_password) == 0) {
                    handle_successful_login(page->app_widgets, username);
                } else {
                    show_error_dialog(page->app_widgets->window, "Invalid username or password");
                }
            }
        } else {
            show_error_dialog(page->app_widgets->window, "Invalid username or password");
        }
        PQclear(res);
    }
}

void on_register_nav_button_clicked(GtkButton *button, gpointer user_data) {
    LoginPage *page = (LoginPage *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(page->app_widgets->stack), "register");
}

LoginPage* login_page_new(AppWidgets *app_widgets) {
    LoginPage *page = malloc(sizeof(LoginPage));
    page->app_widgets = app_widgets;
    
    // Create main container
    page->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(page->container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page->container, GTK_ALIGN_CENTER);

    // Image logo
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale("icon.png", 500, 500, TRUE, NULL);
    GtkWidget *logo_image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_box_pack_start(GTK_BOX(page->container), logo_image, FALSE, FALSE, 0);
    
    // Title
    GtkWidget *title = gtk_label_new("Login");
    gtk_widget_set_name(title, "login-title");
    gtk_box_pack_start(GTK_BOX(page->container), title, FALSE, FALSE, 0);
    
    // Username field
    page->username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->username_entry), "Username");
    gtk_box_pack_start(GTK_BOX(page->container), page->username_entry, FALSE, FALSE, 0);
    
    // Password field
    page->password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(page->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(page->container), page->password_entry, FALSE, FALSE, 0);
    
    // Login button
    page->login_button = gtk_button_new_with_label("Login");
    gtk_widget_set_name(page->login_button, "login-button");
    g_signal_connect(page->login_button, "clicked", G_CALLBACK(on_login_button_clicked), page);
    gtk_box_pack_start(GTK_BOX(page->container), page->login_button, FALSE, FALSE, 10);
    
    // Register button
    page->register_nav_button = gtk_button_new_with_label("Register");
    gtk_widget_set_name(page->register_nav_button, "register-nav-button");
    g_signal_connect(page->register_nav_button, "clicked", G_CALLBACK(on_register_nav_button_clicked), page);
    gtk_box_pack_start(GTK_BOX(page->container), page->register_nav_button, FALSE, FALSE, 0);
    
    return page;
}

void login_page_free(LoginPage *page) {
    if (page) {
        free(page);
    }
}

GtkWidget* login_page_get_container(LoginPage *page) {
    return page->container;
} 