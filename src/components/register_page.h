#ifndef REGISTER_PAGE_H
#define REGISTER_PAGE_H

#include <gtk/gtk.h>
#include "../types/app_types.h"

typedef struct {
    AppWidgets *app_widgets;
    GtkWidget *container;
    GtkWidget *firstname_entry;
    GtkWidget *lastname_entry;
    GtkWidget *email_entry;
    GtkWidget *password_entry;
    GtkWidget *register_button;
    GtkWidget *back_button;
} RegisterPage;

RegisterPage* register_page_new(AppWidgets *app_widgets);
void register_page_free(RegisterPage *page);
GtkWidget* register_page_get_container(RegisterPage *page);

#endif // REGISTER_PAGE_H 