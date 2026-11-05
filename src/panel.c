#include <gtk/gtk.h>
#include <gtk-layer-shell.h>

static gboolean update_clock(gpointer data) {
    GDateTime *now = g_date_time_new_now_local();
    gchar *text = g_date_time_format(now, "%d/%m  %H:%M");
    gtk_label_set_text(GTK_LABEL(data), text);
    g_free(text);
    g_date_time_unref(now);
    return G_SOURCE_CONTINUE;
}

static void launch_apps(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    GError *error = NULL;
    gchar *argv[] = {"fuzzel", NULL};
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, NULL, &error)) {
        g_warning("Não foi possível abrir o lançador: %s", error->message);
        g_clear_error(&error);
    }
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        g_printerr("Execute jlinux-panel dentro de uma sessão Wayland.\n");
        return 1;
    }
    if (!gtk_layer_is_supported()) {
        g_printerr("O compositor precisa suportar wlr-layer-shell (ex.: Sway).\n");
        return 1;
    }
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "JLinux Panel");
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_namespace(GTK_WINDOW(window), "jlinux-panel");
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(window));
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 6);
    gtk_container_add(GTK_CONTAINER(window), box);
    GtkWidget *launcher = gtk_button_new_with_label("JLinux · Aplicativos");
    g_signal_connect(launcher, "clicked", G_CALLBACK(launch_apps), NULL);
    gtk_box_pack_start(GTK_BOX(box), launcher, FALSE, FALSE, 0);
    GtkWidget *clock = gtk_label_new(NULL);
    gtk_box_pack_end(GTK_BOX(box), clock, FALSE, FALSE, 8);
    update_clock(clock);
    guint timer = g_timeout_add_seconds(30, update_clock, clock);
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "window { background: #101820; color: #edf2f7; }"
        "button { background: #203448; color: #edf2f7; border: 0; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    gtk_main();
    g_source_remove(timer);
    return 0;
}
