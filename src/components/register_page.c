#include <gtk/gtk.h>
#include "../types/app_types.h"
#include "register_page.h"
#include <string.h>
#include "../database/db_connection.h"
#include "../security/encryption.h"
#include "../network/protocol.h"
#include "../utils/string_utils.h"
#include <stdlib.h>

static void on_back_button_clicked(GtkButton *button, gpointer user_data) {
    RegisterPage *page = (RegisterPage *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(page->app_widgets->stack), "login");
}

static void on_register_button_clicked(GtkButton *button, gpointer user_data) {
    RegisterPage *page = (RegisterPage *)user_data;
    const gchar *firstname = gtk_entry_get_text(GTK_ENTRY(page->firstname_entry));
    const gchar *lastname = gtk_entry_get_text(GTK_ENTRY(page->lastname_entry));
    const gchar *email = gtk_entry_get_text(GTK_ENTRY(page->email_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(page->password_entry));

    if (strlen(firstname) > 0 && strlen(lastname) > 0 && strlen(email) > 0 && strlen(password) > 0) {
        // Check if email already exists
        const char *check_query = "SELECT user_id FROM users WHERE email = $1";
        const char *check_params[1] = {email};
        PGresult *check_res = PQexecParams(page->app_widgets->db_conn, check_query, 1, NULL, check_params, NULL, NULL, 0);
        
        if (PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
            show_error_dialog(page->app_widgets->window, "Email already exists");
            PQclear(check_res);
            return;
        }
        PQclear(check_res);
        
        // Encrypt password
        char encrypted_password[256];
        size_t password_len = strlen(password);
        xor_encrypt(password, encrypted_password, password_len);
        
        // Insert new user
        const char *insert_query = "INSERT INTO users (first_name, last_name, email, password, status) VALUES ($1, $2, $3, $4, 'offline')";
        const char *insert_params[4] = {firstname, lastname, email, encrypted_password};
        PGresult *insert_res = PQexecParams(page->app_widgets->db_conn, insert_query, 4, NULL, insert_params, NULL, NULL, 0);
        
        if (PQresultStatus(insert_res) == PGRES_COMMAND_OK) {
            gtk_stack_set_visible_child_name(GTK_STACK(page->app_widgets->stack), "login");
        } else {
            show_error_dialog(page->app_widgets->window, "Registration failed");
        }
        PQclear(insert_res);
    }
}

RegisterPage* register_page_new(AppWidgets *app_widgets) {
    RegisterPage *page = malloc(sizeof(RegisterPage));
    page->app_widgets = app_widgets;
    
    // Create main container
    page->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(page->container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page->container, GTK_ALIGN_CENTER);
    
    // Title
    GtkWidget *title = gtk_label_new("Registration");
    gtk_widget_set_name(title, "register-title");
    gtk_box_pack_start(GTK_BOX(page->container), title, FALSE, FALSE, 0);
    
    // First name field
    page->firstname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->firstname_entry), "First name");
    gtk_box_pack_start(GTK_BOX(page->container), page->firstname_entry, FALSE, FALSE, 0);
    
    // Last name field
    page->lastname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->lastname_entry), "Last name");
    gtk_box_pack_start(GTK_BOX(page->container), page->lastname_entry, FALSE, FALSE, 0);
    
    // Email field
    page->email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->email_entry), "Email");
    gtk_box_pack_start(GTK_BOX(page->container), page->email_entry, FALSE, FALSE, 0);
    
    // Password field
    page->password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(page->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(page->container), page->password_entry, FALSE, FALSE, 0);
    
    // Register button
    page->register_button = gtk_button_new_with_label("Register");
    gtk_widget_set_name(page->register_button, "register-submit-button");
    g_signal_connect(page->register_button, "clicked", G_CALLBACK(on_register_button_clicked), page);
    gtk_box_pack_start(GTK_BOX(page->container), page->register_button, FALSE, FALSE, 10);
    
    // Back button
    page->back_button = gtk_button_new_with_label("Back");
    gtk_widget_set_name(page->back_button, "back-button");
    g_signal_connect(page->back_button, "clicked", G_CALLBACK(on_back_button_clicked), page);
    gtk_box_pack_start(GTK_BOX(page->container), page->back_button, FALSE, FALSE, 0);
    
    return page;
}

void register_page_free(RegisterPage *page) {
    if (page) {
        free(page);
    }
}

GtkWidget* register_page_get_container(RegisterPage *page) {
    return page->container;
} 