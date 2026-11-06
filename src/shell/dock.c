#include "dock.h"
#include <glib/gstdio.h>
#include <string.h>
static char *config_path(void) { return g_build_filename(g_get_user_config_dir(), "jlinux", "dock.ini", NULL); }
static gint compare_apps(gconstpointer a, gconstpointer b) {
    return g_utf8_collate(g_app_info_get_display_name(*(GAppInfo *const *)a),
        g_app_info_get_display_name(*(GAppInfo *const *)b));
}
void dock_apps_init(struct dock_apps *dock) {
    dock->apps = g_ptr_array_new_with_free_func(g_object_unref);
    dock->pins = g_ptr_array_new_with_free_func(g_free); dock->writable = TRUE;
    GList *apps = g_app_info_get_all();
    for (GList *item = apps; item; item = item->next) {
        GAppInfo *app = item->data;
        if (G_IS_DESKTOP_APP_INFO(app) && g_app_info_should_show(app) && g_app_info_get_id(app)) {
            g_ptr_array_add(dock->apps, g_object_ref(app));
        }
    }
    g_list_free_full(apps, g_object_unref); g_ptr_array_sort(dock->apps, compare_apps);
    char *path = config_path(); GKeyFile *config = g_key_file_new(); GError *error = NULL;
    GStatBuf metadata;
    if (!g_stat(path, &metadata) && metadata.st_size > 65536) {
        dock->writable = FALSE;
    } else if (g_key_file_load_from_file(config, path, G_KEY_FILE_NONE, &error)) {
        gsize length = 0; char **ids = g_key_file_get_string_list(config, "Dock", "Apps", &length, &error);
        if (!ids || length > 32) { dock->writable = FALSE; }
        else {
            for (gsize i = 0; i < length; i++) {
                if (!*ids[i] || strlen(ids[i]) > 255 || strchr(ids[i], '/') || strchr(ids[i], '\n')) { dock->writable = FALSE; break; }
                if (!g_ptr_array_find_with_equal_func(dock->pins, ids[i], g_str_equal, NULL)) { g_ptr_array_add(dock->pins, g_strdup(ids[i])); }
            }
        }
        g_strfreev(ids);
    } else if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) { dock->writable = FALSE; }
    if (!dock->writable) { g_ptr_array_set_size(dock->pins, 0); g_warning("dock.ini inválido; arquivo preservado e edição desativada."); }
    g_clear_error(&error); g_key_file_unref(config); g_free(path);
}
gboolean dock_apps_pinned(const struct dock_apps *dock, GAppInfo *app) {
    return g_ptr_array_find_with_equal_func(dock->pins, g_app_info_get_id(app), g_str_equal, NULL);
}
gboolean dock_apps_toggle(struct dock_apps *dock, GAppInfo *app, GError **error) {
    const char *id = g_app_info_get_id(app); guint position = 0;
    gboolean pinned = g_ptr_array_find_with_equal_func(dock->pins, id, g_str_equal, &position);
    if (!dock->writable || (!pinned && dock->pins->len >= 32)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Configuração inválida ou limite de 32 aplicativos atingido."); return FALSE;
    }
    GPtrArray *next = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; i < dock->pins->len; i++) {
        if (!pinned || i != position) { g_ptr_array_add(next, g_strdup(g_ptr_array_index(dock->pins, i))); }
    }
    if (!pinned) { g_ptr_array_add(next, g_strdup(id)); }
    GKeyFile *config = g_key_file_new();
    g_key_file_set_string_list(config, "Dock", "Apps", (const char *const *)next->pdata, next->len);
    char *data = g_key_file_to_data(config, NULL, NULL), *path = config_path(), *parent = g_path_get_dirname(path);
    gboolean saved = g_mkdir_with_parents(parent, 0700) == 0;
    if (!saved) { g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Não foi possível criar a pasta de configuração."); }
    else { saved = g_file_set_contents_full(path, data, -1, G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE, 0600, error); }
    if (saved) { g_ptr_array_unref(dock->pins); dock->pins = next; }
    else { g_ptr_array_unref(next); }
    g_free(parent); g_free(path); g_free(data); g_key_file_unref(config); return saved;
}
void dock_apps_finish(struct dock_apps *dock) { g_ptr_array_unref(dock->apps); g_ptr_array_unref(dock->pins); }
