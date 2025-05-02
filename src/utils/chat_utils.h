#ifndef CHAT_UTILS_H
#define CHAT_UTILS_H

#include <gtk/gtk.h>
#include "../types/app_types.h"

// Function to store a message in the database
bool store_message(AppWidgets *widgets, const char *message, uint32_t channel_id);

// Function to update chat history with a new message
void update_chat_history(AppWidgets *widgets, const char *sender, const char *message, const char *channel_name);

// Function to update chat history from network messages
gboolean update_chat_history_from_network(gpointer data);

// Function to handle successful login
void handle_successful_login(AppWidgets *widgets, const char *username);

// Function to fetch the display name ("First Last") from DB
void get_display_name(AppWidgets *widgets, const char *sender_email, char *display_name, size_t size);

// Function to refresh the channel list
void refresh_channel_list(AppWidgets *widgets);

// Function to ensure user has proper channel associations
void ensure_user_channel_associations(AppWidgets *widgets, const char *user_id);

// Function to load channel history
void load_channel_history(AppWidgets *widgets, uint32_t channel_id);

// Function to format timestamp (potentially move definition too)
void format_timestamp(const char *db_timestamp, char *formatted_time, size_t size);

// Function to update user status in the database
void update_user_status(AppWidgets *widgets, const char *username, const char *status);

#endif 