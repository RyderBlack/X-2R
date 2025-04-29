#include <gtk/gtk.h>
#include "../types/app_types.h"
#include "register_page.h"
#include <string.h>
#include "../database/db_connection.h"
#include "../security/encryption.h"
#include "../network/protocol.h"
#include "../utils/string_utils.h"

static void on_back_button_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "login");
}

static void on_register_button_clicked(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;
    
    const char *firstname = gtk_entry_get_text(GTK_ENTRY(widgets->register_firstname_entry));
    const char *lastname = gtk_entry_get_text(GTK_ENTRY(widgets->register_lastname_entry));
    const char *email = gtk_entry_get_text(GTK_ENTRY(widgets->register_email_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(widgets->register_password_entry));
    
    if (!firstname || !lastname || !email || !password ||
        strlen(firstname) == 0 || strlen(lastname) == 0 ||
        strlen(email) == 0 || strlen(password) == 0) {
        show_error_dialog(widgets->window, "Please fill in all fields");
        return;
    }
    
    // Create registration message
    Message *reg_msg = create_registration_message(firstname, lastname, email, password);
    if (!reg_msg) {
        show_error_dialog(widgets->window, "Failed to create registration message");
        return;
    }
    
    if (send_message(widgets->server_socket, reg_msg) < 0) {
        show_error_dialog(widgets->window, "Failed to send registration message");
        free(reg_msg);
        return;
    }
    
    free(reg_msg);
}

GtkWidget* create_register_page(AppWidgets *widgets) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    
    // First name entry
    GtkWidget *firstname_label = gtk_label_new("First Name:");
    widgets->register_firstname_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), firstname_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), widgets->register_firstname_entry, FALSE, FALSE, 0);
    
    // Last name entry
    GtkWidget *lastname_label = gtk_label_new("Last Name:");
    widgets->register_lastname_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), lastname_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), widgets->register_lastname_entry, FALSE, FALSE, 0);
    
    // Email entry
    GtkWidget *email_label = gtk_label_new("Email:");
    widgets->register_email_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), email_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), widgets->register_email_entry, FALSE, FALSE, 0);
    
    // Password entry
    GtkWidget *password_label = gtk_label_new("Password:");
    widgets->register_password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(widgets->register_password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), password_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), widgets->register_password_entry, FALSE, FALSE, 0);
    
    // Register button
    GtkWidget *register_button = gtk_button_new_with_label("Register");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), register_button, FALSE, FALSE, 0);
    
    // Back button
    GtkWidget *back_button = gtk_button_new_with_label("Back to Login");
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), back_button, FALSE, FALSE, 0);
    
    return box;
} 