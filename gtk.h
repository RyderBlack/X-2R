// gtk.h – Ton header central pour les fonctions liées à l'interface GTK

#ifndef GTK_WRAPPER_H
#define GTK_WRAPPER_H

#include <gtk/gtk.h>

// === Fonctions pour initialiser l'application ===
void activate_login(GtkApplication *app, gpointer user_data);
void activate_home(GtkApplication *app, gpointer user_data);

// === Utilitaires ===
GtkWidget* create_login_window(GtkApplication *app);
GtkWidget* create_home_window(GtkApplication *app);

// === CSS Styling ===
void load_css(void);

#endif // GTK_WRAPPER_H
