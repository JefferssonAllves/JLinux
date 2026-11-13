#define _GNU_SOURCE
#include <stdbool.h>
#include "apps.h"
#include <gio/gdesktopappinfo.h>
#include <librsvg/rsvg.h>
#include <signal.h>
#include <string.h>
static const struct {const char *name, *desktop, *icon;} apps[] = {
    {"Brave", "brave-browser.desktop", "brave-desktop"},
    {"Terminal", "foot.desktop", "foot"},
    {"Neovim", "Neovim-Code.desktop", "nvim"},
    {"qBittorrent", "org.qbittorrent.qBittorrent.desktop", "qbittorrent"},
    {"Arquivos", "org.jlinux.Files.desktop", "system-file-manager"},
    {"Configurações", NULL, "preferences-system"},
    {"Imagens", "org.jlinux.Files.desktop", "folder-pictures"},
    {"Vídeos", "org.jlinux.Files.desktop", "folder-videos"},
    {"Monitor", "btop.desktop", "btop"},
    {"Spotube", "com.github.KRTirtho.Spotube.desktop", "spotify"},
    {"WhatsApp", "org.jlinux.WhatsApp.desktop", "org.jlinux.WhatsApp"},
};
const char *dock_app_name(int index) { return index >= 0 && index < 11 ? apps[index].name : ""; }
static cairo_surface_t *load_icon(const char *path) {
    if (!path || !g_file_test(path, G_FILE_TEST_IS_REGULAR)) { return NULL; }
    cairo_surface_t *image;
    if (g_str_has_suffix(path, ".svg")) {
        RsvgHandle *svg = rsvg_handle_new_from_file(path, NULL);
        if (!svg) { return NULL; }
        image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 128, 128);
        cairo_t *cr = cairo_create(image);
        bool ok = rsvg_handle_render_document(svg, cr, &(RsvgRectangle){0,0,128,128}, NULL);
        cairo_destroy(cr); g_object_unref(svg);
        if (!ok) { cairo_surface_destroy(image); return NULL; }
    } else { image = cairo_image_surface_create_from_png(path); }
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(image); return NULL; }
    return image;
}
static cairo_surface_t *find_icon(const char *name) {
    if (g_path_is_absolute(name)) { return load_icon(name); }
    const char *themes[] = {"hicolor", "Adwaita", "AdwaitaLegacy"};
    const char *sizes[] = {"128x128", "scalable", "96x96", "64x64", "48x48", "32x32"};
    const char *contexts[] = {"apps", "places", "categories", "devices", "actions", "legacy"};
    const char *extensions[] = {"png", "svg"};
    const char * const *system = g_get_system_data_dirs();
    for (int base = -1; base < (int)g_strv_length((char **)system); base++) {
        const char *directory = base < 0 ? g_get_user_data_dir() : system[base];
        for (unsigned t=0;t<G_N_ELEMENTS(themes);t++) for(unsigned s=0;s<G_N_ELEMENTS(sizes);s++)
        for(unsigned c=0;c<G_N_ELEMENTS(contexts);c++) for(unsigned e=0;e<2;e++) {
            char *path = g_strdup_printf("%s/icons/%s/%s/%s/%s.%s", directory, themes[t], sizes[s], contexts[c], name, extensions[e]);
            cairo_surface_t *image = load_icon(path); g_free(path);
            if (image) { return image; }
        }
    }
    return NULL;
}
static cairo_surface_t *settings_icon(void) {
    cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,128,128);
    cairo_t *cr=cairo_create(image);cairo_translate(cr,64,64);
    cairo_set_source_rgb(cr,.70,.80,.96);
    for(int i=0;i<10;i++){
        cairo_save(cr);cairo_rotate(cr,i*G_PI/5);cairo_rectangle(cr,-9,-59,18,28);cairo_fill(cr);cairo_restore(cr);
    }
    cairo_arc(cr,0,0,48,0,2*G_PI);cairo_fill(cr);
    cairo_set_source_rgb(cr,.025,.035,.065);cairo_arc(cr,0,0,31,0,2*G_PI);cairo_fill(cr);
    cairo_set_source_rgb(cr,.65,.83,1);cairo_set_line_width(cr,5);cairo_arc(cr,0,0,17,0,2*G_PI);cairo_stroke(cr);
    cairo_destroy(cr);
    if(cairo_surface_status(image)!=CAIRO_STATUS_SUCCESS){cairo_surface_destroy(image);return NULL;}
    return image;
}
cairo_surface_t *dock_app_icon(int index) {
    if (index < 0 || index >= 11) { return NULL; }
    if (index == 5) { return settings_icon(); }
    /* Folder shortcuts use their folder icons rather than the file manager's logo. */
    GDesktopAppInfo *info = apps[index].desktop ? g_desktop_app_info_new(apps[index].desktop) : NULL;
    GIcon *icon = info && index != 6 && index != 7 ? g_app_info_get_icon(G_APP_INFO(info)) : NULL;
    cairo_surface_t *image = NULL;
    if (icon && G_IS_FILE_ICON(icon)) { image = load_icon(g_file_peek_path(g_file_icon_get_file(G_FILE_ICON(icon)))); }
    if (icon && G_IS_THEMED_ICON(icon)) {
        const char * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        for (int n=0; names[n] && !image; n++) { image = find_icon(names[n]); }
    }
    if (!image) { image = find_icon(apps[index].icon); }
    if (info) { g_object_unref(info); }
    return image;
}
static void reset_signals(gpointer data) { (void)data; sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, NULL); }
bool dock_app_launch(int index, GError **error) {
    if (index < 0 || index >= 11 || index == 5) { return false; }
    if (index == 1 || index == 8) {
        char *terminal[] = {"foot", "--term=xterm-256color", "--title=JLinux Terminal", NULL, NULL, NULL};
        if (index == 8) { terminal[3] = "-e"; terminal[4] = "btop"; }
        return g_spawn_async(NULL, terminal, NULL, G_SPAWN_SEARCH_PATH, reset_signals, NULL, NULL, error);
    }
    GDesktopAppInfo *info = g_desktop_app_info_new(apps[index].desktop);
    if (!info) { g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Aplicativo não instalado: %s", apps[index].name); return false; }
    GList *files = NULL;
    if (index == 6 || index == 7) {
        const char *directory = g_get_user_special_dir(index == 6 ? G_USER_DIRECTORY_PICTURES : G_USER_DIRECTORY_VIDEOS);
        files = g_list_append(files, g_file_new_for_path(directory ? directory : g_get_home_dir()));
    }
    GList *uris = files ? g_list_append(NULL, g_file_get_uri(files->data)) : NULL;
    bool ok = g_desktop_app_info_launch_uris_as_manager(info, uris, NULL,
        G_SPAWN_SEARCH_PATH, reset_signals, NULL, NULL, NULL, error);
    g_list_free_full(uris, g_free);
    g_list_free_full(files, g_object_unref); g_object_unref(info); return ok;
}
