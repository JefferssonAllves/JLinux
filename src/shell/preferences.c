#include <stdbool.h>
#include "preferences.h"
#include <glib/gstdio.h>
#include <errno.h>
struct shell_preferences shell_preferences_default(void) { return (struct shell_preferences){true,true,true,true,true,true}; }
static char *path(void) { return g_build_filename(g_get_user_config_dir(), "jlinux", "shell.ini", NULL); }
bool shell_preferences_load(struct shell_preferences *prefs, GError **error) {
    char *filename = path(); GKeyFile *file = g_key_file_new(); GError *local = NULL;
    bool ok = g_key_file_load_from_file(file, filename, G_KEY_FILE_NONE, &local);
    if (!ok) {
        bool missing = g_error_matches(local, G_FILE_ERROR, G_FILE_ERROR_NOENT);
        if (missing) { g_clear_error(&local); } else { g_propagate_error(error, local); }
        g_free(filename); g_key_file_unref(file); return missing;
    }
    struct shell_preferences next = *prefs;
    const char *keys[] = {"animations", "zoom", "neon", "tint", "reflection", "shadow"};
    bool *values[] = {&next.animations,&next.zoom,&next.neon,&next.tint,&next.reflection,&next.shadow};
    for (unsigned i=0;i<G_N_ELEMENTS(keys);i++) {
        if (g_key_file_has_key(file,"Shell",keys[i],NULL)) {
            *values[i] = g_key_file_get_boolean(file,"Shell",keys[i],&local);
            if (local) { g_propagate_error(error, local); ok = false; break; }
        }
    }
    if (ok) { *prefs = next; }
    g_free(filename); g_key_file_unref(file); return ok;
}
bool shell_preferences_save(const struct shell_preferences *prefs, GError **error) {
    char *filename = path(), *directory = g_path_get_dirname(filename);
    if (g_mkdir_with_parents(directory,0700)) {
        g_set_error(error,G_FILE_ERROR,g_file_error_from_errno(errno),"Não foi possível criar a pasta de configurações");
        g_free(filename);g_free(directory);return false;
    }
    GKeyFile *file = g_key_file_new();
    const char *keys[] = {"animations","zoom","neon","tint","reflection","shadow"};
    bool values[] = {prefs->animations,prefs->zoom,prefs->neon,prefs->tint,prefs->reflection,prefs->shadow};
    for(unsigned i=0;i<G_N_ELEMENTS(keys);i++){g_key_file_set_boolean(file,"Shell",keys[i],values[i]);}
    gsize length; char *data = g_key_file_to_data(file,&length,NULL);
    bool ok = g_file_set_contents_full(filename,data,length,G_FILE_SET_CONTENTS_CONSISTENT,0600,error);
    g_free(data);g_key_file_unref(file);g_free(filename);g_free(directory);return ok;
}
