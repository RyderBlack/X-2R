#include <gtk/gtk.h>
#include <string.h>

// Structure pour stocker les widgets principaux
typedef struct
{
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *login_box;
    GtkWidget *chat_box;
    GtkWidget *register_box;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *register_firstname_entry;
    GtkWidget *register_lastname_entry;
    GtkWidget *register_email_entry;
    GtkWidget *register_password_entry;
    GtkWidget *chat_input;
    GtkWidget *chat_history;
    GtkWidget *chat_channels_list;
    GtkWidget *contacts_list;
} AppWidgets;

// Fonction de callback pour le bouton de connexion
static void on_login_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(widgets->username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(widgets->password_entry));

    // Ici, vous pouvez ajouter votre logique d'authentification
    if (strlen(username) > 0 && strlen(password) > 0)
    {
        gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "chat");
    }
}

// Fonction de callback pour envoyer un message
static void on_send_message(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *message = gtk_entry_get_text(GTK_ENTRY(widgets->chat_input));

    if (strlen(message) > 0)
    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->chat_history));
        GtkTextIter end;

        gtk_text_buffer_get_end_iter(buffer, &end);
        gtk_text_buffer_insert(buffer, &end, "Vous: ", -1);
        gtk_text_buffer_insert(buffer, &end, message, -1);
        gtk_text_buffer_insert(buffer, &end, "\n", -1);

        gtk_entry_set_text(GTK_ENTRY(widgets->chat_input), "");
    }
}

// Fonction de callback pour le bouton d'inscription sur la page login
static void on_register_nav_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "register");
}

// Fonction de callback pour le bouton d'inscription sur la page d'inscription
static void on_register_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    const gchar *firstname = gtk_entry_get_text(GTK_ENTRY(widgets->register_firstname_entry));
    const gchar *lastname = gtk_entry_get_text(GTK_ENTRY(widgets->register_lastname_entry));
    const gchar *email = gtk_entry_get_text(GTK_ENTRY(widgets->register_email_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(widgets->register_password_entry));

    // Ici, vous pouvez ajouter votre logique d'inscription
    if (strlen(firstname) > 0 && strlen(lastname) > 0 && strlen(email) > 0 && strlen(password) > 0)
    {
        // Retour à la page de connexion après inscription
        gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "login");
    }
}

// Fonction de callback pour le bouton de retour
static void on_back_button_clicked(GtkButton *button, gpointer user_data)
{
    AppWidgets *widgets = (AppWidgets *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->stack), "login");
}

// Fonction pour créer la page de connexion
static GtkWidget *create_login_page(AppWidgets *widgets)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // Titre
    GtkWidget *title = gtk_label_new("Connexion");
    gtk_widget_set_name(title, "login-title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    // Champ nom d'utilisateur
    widgets->username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->username_entry), "Nom d'utilisateur");
    gtk_box_pack_start(GTK_BOX(box), widgets->username_entry, FALSE, FALSE, 0);

    // Champ mot de passe
    widgets->password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->password_entry), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(widgets->password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), widgets->password_entry, FALSE, FALSE, 0);

    // Bouton de connexion
    GtkWidget *login_button = gtk_button_new_with_label("Se connecter");
    gtk_widget_set_name(login_button, "login-button");
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), login_button, FALSE, FALSE, 10);

    // Bouton d'inscription
    GtkWidget *register_nav_button = gtk_button_new_with_label("S'inscrire");
    gtk_widget_set_name(register_nav_button, "register-nav-button");
    g_signal_connect(register_nav_button, "clicked", G_CALLBACK(on_register_nav_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), register_nav_button, FALSE, FALSE, 0);

    return box;
}

// Fonction pour créer la page d'inscription
static GtkWidget *create_register_page(AppWidgets *widgets)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // Titre
    GtkWidget *title = gtk_label_new("Inscription");
    gtk_widget_set_name(title, "register-title");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    // Champ prénom
    widgets->register_firstname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_firstname_entry), "Prénom");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_firstname_entry, FALSE, FALSE, 0);

    // Champ nom
    widgets->register_lastname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_lastname_entry), "Nom");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_lastname_entry, FALSE, FALSE, 0);

    // Champ email
    widgets->register_email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_email_entry), "Email");
    gtk_box_pack_start(GTK_BOX(box), widgets->register_email_entry, FALSE, FALSE, 0);

    // Champ mot de passe
    widgets->register_password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->register_password_entry), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(widgets->register_password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), widgets->register_password_entry, FALSE, FALSE, 0);

    // Bouton d'inscription
    GtkWidget *register_button = gtk_button_new_with_label("S'inscrire");
    gtk_widget_set_name(register_button, "register-button");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), register_button, FALSE, FALSE, 10);

    // Bouton de retour
    GtkWidget *back_button = gtk_button_new_with_label("Retour");
    gtk_widget_set_name(back_button, "back-button");
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_button_clicked), widgets);
    gtk_box_pack_start(GTK_BOX(box), back_button, FALSE, FALSE, 0);

    return box;
}

// Fonction pour créer la page de discussion (comme Discord)
static GtkWidget *create_chat_page(AppWidgets *widgets)
{
    // Container principal
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // 1. Liste des canaux (à gauche)
    GtkWidget *channels_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(channels_box, 200, -1);
    gtk_widget_set_name(channels_box, "channels-box");

    GtkWidget *channels_title = gtk_label_new("Discussions");
    gtk_widget_set_name(channels_title, "channels-title");
    gtk_box_pack_start(GTK_BOX(channels_box), channels_title, FALSE, FALSE, 10);

    widgets->chat_channels_list = gtk_list_box_new();
    gtk_widget_set_name(widgets->chat_channels_list, "channels-list");

    // Ajout de quelques canaux d'exemple
    GtkWidget *channel1 = gtk_label_new("# général");
    gtk_widget_set_halign(channel1, GTK_ALIGN_START);
    gtk_widget_set_margin_start(channel1, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), channel1, -1);

    GtkWidget *channel2 = gtk_label_new("# aide");
    gtk_widget_set_halign(channel2, GTK_ALIGN_START);
    gtk_widget_set_margin_start(channel2, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), channel2, -1);

    GtkWidget *channel3 = gtk_label_new("# annonces");
    gtk_widget_set_halign(channel3, GTK_ALIGN_START);
    gtk_widget_set_margin_start(channel3, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->chat_channels_list), channel3, -1);

    GtkWidget *channels_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(channels_scroll), widgets->chat_channels_list);
    gtk_box_pack_start(GTK_BOX(channels_box), channels_scroll, TRUE, TRUE, 0);

    // 2. Zone centrale (historique des messages et zone de saisie)
    GtkWidget *chat_center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(chat_center_box, TRUE);

    // 2.1 Historique des messages
    widgets->chat_history = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(widgets->chat_history), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(widgets->chat_history), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_name(widgets->chat_history, "chat-history");

    // Ajout de quelques messages d'exemple
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets->chat_history));
    gtk_text_buffer_set_text(buffer, "Bienvenue dans #général\n\nUtilisateur1: Bonjour tout le monde!\n\nUtilisateur2: Salut! Comment ça va?\n\n", -1);

    GtkWidget *history_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(history_scroll), widgets->chat_history);
    gtk_widget_set_vexpand(history_scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(chat_center_box), history_scroll, TRUE, TRUE, 0);

    // 2.2 Zone de saisie
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(input_box, 10);
    gtk_widget_set_margin_end(input_box, 10);
    gtk_widget_set_margin_top(input_box, 10);
    gtk_widget_set_margin_bottom(input_box, 10);

    widgets->chat_input = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->chat_input), "Envoyer un message à #général");
    gtk_widget_set_name(widgets->chat_input, "chat-input");
    gtk_widget_set_hexpand(widgets->chat_input, TRUE);

    GtkWidget *send_button = gtk_button_new_with_label("Envoyer");
    gtk_widget_set_name(send_button, "send-button");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_message), widgets);

    gtk_box_pack_start(GTK_BOX(input_box), widgets->chat_input, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_box), send_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(chat_center_box), input_box, FALSE, FALSE, 0);

    // 3. Liste des contacts (à droite)
    GtkWidget *contacts_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(contacts_box, 200, -1);
    gtk_widget_set_name(contacts_box, "contacts-box");

    GtkWidget *contacts_title = gtk_label_new("Contacts");
    gtk_widget_set_name(contacts_title, "contacts-title");
    gtk_box_pack_start(GTK_BOX(contacts_box), contacts_title, FALSE, FALSE, 10);

    widgets->contacts_list = gtk_list_box_new();
    gtk_widget_set_name(widgets->contacts_list, "contacts-list");

    // Ajout de quelques contacts d'exemple
    GtkWidget *contact1 = gtk_label_new("Utilisateur1");
    gtk_widget_set_halign(contact1, GTK_ALIGN_START);
    gtk_widget_set_margin_start(contact1, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->contacts_list), contact1, -1);

    GtkWidget *contact2 = gtk_label_new("Utilisateur2");
    gtk_widget_set_halign(contact2, GTK_ALIGN_START);
    gtk_widget_set_margin_start(contact2, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->contacts_list), contact2, -1);

    GtkWidget *contact3 = gtk_label_new("Utilisateur3");
    gtk_widget_set_halign(contact3, GTK_ALIGN_START);
    gtk_widget_set_margin_start(contact3, 10);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->contacts_list), contact3, -1);

    GtkWidget *contacts_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(contacts_scroll), widgets->contacts_list);
    gtk_box_pack_start(GTK_BOX(contacts_box), contacts_scroll, TRUE, TRUE, 0);

    // Assembler les trois parties
    gtk_box_pack_start(GTK_BOX(main_box), channels_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), chat_center_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), contacts_box, FALSE, FALSE, 0);

    return main_box;
}

// Fonction principale
int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    // Création de la fenêtre principale
    AppWidgets widgets;
    widgets.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(widgets.window), "Diskorde");
    gtk_window_set_default_size(GTK_WINDOW(widgets.window), 1000, 600);
    g_signal_connect(widgets.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Création du stack pour gérer les pages
    widgets.stack = gtk_stack_new();
    gtk_container_add(GTK_CONTAINER(widgets.window), widgets.stack);

    // Création des pages
    widgets.login_box = create_login_page(&widgets);
    widgets.chat_box = create_chat_page(&widgets);
    widgets.register_box = create_register_page(&widgets);

    // Ajout des pages au stack
    gtk_stack_add_named(GTK_STACK(widgets.stack), widgets.login_box, "login");
    gtk_stack_add_named(GTK_STACK(widgets.stack), widgets.chat_box, "chat");
    gtk_stack_add_named(GTK_STACK(widgets.stack), widgets.register_box, "register");

    // Définition de la page de connexion comme page par défaut
    gtk_stack_set_visible_child_name(GTK_STACK(widgets.stack), "login");

    // Chargement du CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
                                    "window { background-color: black; }"
                                    "#login-title, #register-title { font-size: 24px; color: red; margin-bottom: 20px; }"
                                    "#channels-title, #contacts-title { font-size: 18px; color: red; font-weight: bold; }"
                                    "entry { padding: 8px; border-radius: 4px; border: 1px solid red; color: white; background-color: black; margin: 5px 0; }"
                                    "entry:focus { border-color: #ff3333; }"
                                    "#login-button, #register-button, #register-nav-button, #back-button, #send-button { padding: 10px 20px; background-color: red; color: red; border: none; border-radius: 4px; }"
                                    "#login-button:hover, #register-button:hover, #register-nav-button:hover, #back-button:hover, #send-button:hover { background-color: #cc0000; }"
                                    "#chat-history { background-color: #111111; color: white; }"
                                    "#channels-box, #contacts-box { background-color: #1a1a1a; border-right: 1px solid #333333; border-left: 1px solid #333333; }"
                                    "#channels-list, #contacts-list { background-color: #1a1a1a; color: white; }"
                                    "#channels-list label, #contacts-list label { padding: 8px 0; }"
                                    "#channels-list label:hover, #contacts-list label:hover { background-color: #333333; }",
                                    -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_widget_show_all(widgets.window);
    gtk_main();

    return 0;
}