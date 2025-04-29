#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include <gtk/gtk.h>
#include "../types/app_types.h"

// Function declarations
GtkWidget *create_login_page(AppWidgets *widgets);
void on_login_button_clicked(GtkWidget *widget, gpointer data);
void on_register_nav_button_clicked(GtkWidget *widget, gpointer data);

#endif // LOGIN_PAGE_H 