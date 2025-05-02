#include <gtk/gtk.h>
#include "../types/app_types.h"
#include "register_page.h"
#include <string.h>
#include "../network/protocol.h"
#include "../utils/string_utils.h"
#include <stdlib.h>

// Need utils for show_error_dialog
#include "../utils/chat_utils.h"

static void on_back_button_clicked(GtkButton *button, gpointer user_data) {
    RegisterPage *page = (RegisterPage *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(page->app_widgets->stack), "login");
}

static void on_register_button_clicked(GtkButton *button, gpointer user_data) {
    RegisterPage *page = (RegisterPage *)user_data;
    AppWidgets *widgets = page->app_widgets; // Get AppWidgets

    const gchar *firstname = gtk_entry_get_text(GTK_ENTRY(page->firstname_entry));
    const gchar *lastname = gtk_entry_get_text(GTK_ENTRY(page->lastname_entry));
    const gchar *email = gtk_entry_get_text(GTK_ENTRY(page->email_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(page->password_entry));

    // Basic client-side validation
    if (strlen(firstname) == 0 || strlen(lastname) == 0 || strlen(email) == 0 || strlen(password) == 0) {
        show_error_dialog(widgets->window, "All fields are required");
        return;
    }

    // --- Send Registration Request to Server --- //
    printf("🔒 Sending registration request for email: %s\n", email);
    
    RegisterRequest reg_req;
    memset(&reg_req, 0, sizeof(RegisterRequest));
    strncpy(reg_req.firstname, firstname, sizeof(reg_req.firstname) - 1);
    strncpy(reg_req.lastname, lastname, sizeof(reg_req.lastname) - 1);
    strncpy(reg_req.email, email, sizeof(reg_req.email) - 1);
    strncpy(reg_req.password, password, sizeof(reg_req.password) - 1); // Send plain text
    // Ensure null termination
    reg_req.firstname[sizeof(reg_req.firstname) - 1] = '\0';
    reg_req.lastname[sizeof(reg_req.lastname) - 1] = '\0';
    reg_req.email[sizeof(reg_req.email) - 1] = '\0';
    reg_req.password[sizeof(reg_req.password) - 1] = '\0';

    Message *msg = create_message(MSG_REGISTER_REQUEST, &reg_req, sizeof(RegisterRequest));
    if (!msg) {
        fprintf(stderr, "❌ Failed to create registration request message\n");
        show_error_dialog(widgets->window, "Registration failed: Could not create request");
        return;
    }

    if (send_message(widgets->server_socket, msg) < 0) {
        fprintf(stderr, "❌ Failed to send registration request message\n");
        show_error_dialog(widgets->window, "Registration failed: Could not send request to server");
    }

    free(msg);
    // --- Request Sent - Wait for Response in receive_messages --- //
    // The response from the server (MSG_REGISTER_SUCCESS/FAILURE)
    // will trigger the next action (e.g., showing dialog, switching page).
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