#ifndef CHAT_WINDOW_H
#define CHAT_WINDOW_H

#include <gtk/gtk.h>

// Initializes the main chat window
// `send_callback` is called when the user hits "Send"
// `userdata` is passed to the callback
void start_chat_window(void (*send_callback)(const gchar *message, gpointer), gpointer userdata);

// Call this from receive thread via `g_idle_add()` to append new message
gboolean append_message_to_chat(gpointer message);

#endif // CHAT_WINDOW_H
