#define _GNU_SOURCE
#include <stdbool.h>
#include "apps.h"
#include <gio/gdesktopappinfo.h>
#include <librsvg/rsvg.h>
#include <signal.h>
#include <string.h>
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
    if (index == 9) { return find_icon("org.jlinux.Spotube"); }
    /* Folder shortcuts use their folder icons rather than the file manager's logo. */
    GDesktopAppInfo *info = dock_app_desktop(index) ? g_desktop_app_info_new(dock_app_desktop(index)) : NULL;
    GIcon *icon = info && index != 6 && index != 7 ? g_app_info_get_icon(G_APP_INFO(info)) : NULL;
    cairo_surface_t *image = NULL;
    if (icon && G_IS_FILE_ICON(icon)) { image = load_icon(g_file_peek_path(g_file_icon_get_file(G_FILE_ICON(icon)))); }
    if (icon && G_IS_THEMED_ICON(icon)) {
        const char * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        for (int n=0; names[n] && !image; n++) { image = find_icon(names[n]); }
    }
    if (!image) { image = find_icon(dock_app_icon_name(index)); }
    if (info) { g_object_unref(info); }
    return image;
}
