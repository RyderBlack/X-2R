#ifndef CHAT_PAGE_H
#define CHAT_PAGE_H

#include <gtk/gtk.h>
#include "../types/app_types.h"

typedef struct {
    AppWidgets *app_widgets;
    GtkWidget *container;
    GtkWidget *chat_input;
    GtkWidget *chat_history;
    GtkWidget *chat_channels_list;
    GtkWidget *contacts_list;
    GtkWidget *channel_name;
    GtkWidget *logout_button;
} ChatPage;

ChatPage* chat_page_new(AppWidgets *app_widgets);
void chat_page_free(ChatPage *page);
GtkWidget* chat_page_get_container(ChatPage *page);

#endif // CHAT_PAGE_H 