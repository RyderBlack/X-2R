#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include <gtk/gtk.h>
#include "../types/app_types.h"

typedef struct {
    AppWidgets *app_widgets;
    GtkWidget *container;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *login_button;
    GtkWidget *register_nav_button;
} LoginPage;

LoginPage* login_page_new(AppWidgets *app_widgets);
void login_page_free(LoginPage *page);
GtkWidget* login_page_get_container(LoginPage *page);
void on_login_button_clicked(GtkButton *button, gpointer user_data);
void on_register_nav_button_clicked(GtkButton *button, gpointer user_data);

// Function declarations
GtkWidget *create_login_page(AppWidgets *widgets);

#endif // LOGIN_PAGE_H 