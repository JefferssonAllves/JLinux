#include <gtk/gtk.h>
#include <gtk-layer-shell.h>

static gboolean update_clock(gpointer data) {
    GDateTime *now = g_date_time_new_now_local();
    gchar *text = g_date_time_format(now, "%d/%m  ·  %H:%M");
    gtk_label_set_text(GTK_LABEL(data), text);
    g_free(text);
    g_date_time_unref(now);
    return G_SOURCE_CONTINUE;
}

static GtkWidget *label(const char *text, const char *name) {
    GtkWidget *widget = gtk_label_new(text);
    gtk_widget_set_name(widget, name);
    return widget;
}

static GtkWidget *layer_window(const char *name, GtkLayerShellLayer layer) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(window, name);
    gtk_window_set_title(GTK_WINDOW(window), name);
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_namespace(GTK_WINDOW(window), name);
    gtk_layer_set_layer(GTK_WINDOW(window), layer);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    return window;
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv) || !gtk_layer_is_supported()) {
        g_printerr("JLinux Shell requer sessão Wayland com layer-shell.\n");
        return 1;
    }
    GtkCssProvider *css = gtk_css_provider_new();
    GError *error = NULL;
    gtk_css_provider_load_from_data(css,
        "* { font-family: Sans; color: #e5edf7; }"
        "#jlinux-wallpaper { background: #090e17; }"
        "#jlinux-panel { background: #111b29; border-bottom: 1px solid #243449; }"
        "#brand { color: #6ce5c0; font-size: 15px; font-weight: bold; }"
        "#workspace { color: #a9b9cc; font-size: 12px; }"
        "#clock { font-size: 12px; }"
        "#eyebrow { color: #6ce5c0; font-size: 12px; letter-spacing: 3px; }"
        "#title { font-size: 72px; font-weight: bold; }"
        "#subtitle { color: #a9b9cc; font-size: 19px; }"
        "#status { color: #71849b; font-size: 12px; margin-top: 24px; }",
        -1, &error);
    if (error) {
        g_printerr("Tema inválido: %s\n", error->message);
        g_error_free(error);
        g_object_unref(css);
        return 1;
    }
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    const gchar *theme_path = g_getenv("JLINUX_THEME");
    if (theme_path) {
        GtkCssProvider *custom = gtk_css_provider_new();
        if (!gtk_css_provider_load_from_path(custom, theme_path, &error)) {
            g_printerr("Tema adicional inválido: %s\n", error->message);
            g_clear_error(&error);
            g_object_unref(custom);
            return 1;
        }
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(custom), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
        g_object_unref(custom);
    }

    GtkWidget *wallpaper = layer_window("jlinux-wallpaper", GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_anchor(GTK_WINDOW(wallpaper), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    GtkWidget *welcome = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_halign(welcome, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(welcome, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(welcome), label("SEU ESPAÇO", "eyebrow"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(welcome), label("JLinux", "title"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(welcome), label("Uma base feita para ser sua.", "subtitle"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(welcome), label("PRIMEIRO PROTÓTIPO  ·  COMPOSITOR PRÓPRIO", "status"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(wallpaper), welcome);

    GtkWidget *panel = layer_window("jlinux-panel", GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(panel));
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_start(bar, 24);
    gtk_widget_set_margin_end(bar, 24);
    gtk_widget_set_margin_top(bar, 14);
    gtk_widget_set_margin_bottom(bar, 14);
    gtk_box_pack_start(GTK_BOX(bar), label("JLinux", "brand"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), label("Desktop 01", "workspace"), FALSE, FALSE, 0);
    GtkWidget *clock = label("", "clock");
    gtk_box_pack_end(GTK_BOX(bar), clock, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(panel), bar);
    update_clock(clock);
    guint timer = g_timeout_add_seconds(30, update_clock, clock);
    gtk_widget_show_all(wallpaper);
    gtk_widget_show_all(panel);
    gtk_main();
    g_source_remove(timer);
    return 0;
}
