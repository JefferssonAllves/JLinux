#include "blocker.h"

struct filter_load {
    GMainLoop *loop;
    WebKitUserContentFilter *filter;
    GError *error;
};
static void loaded(GObject *store, GAsyncResult *result, gpointer data) {
    struct filter_load *load = data;
    load->filter = webkit_user_content_filter_store_load_finish(WEBKIT_USER_CONTENT_FILTER_STORE(store), result, &load->error);
    g_main_loop_quit(load->loop);
}
static void saved(GObject *store, GAsyncResult *result, gpointer data) {
    struct filter_load *load = data;
    load->filter = webkit_user_content_filter_store_save_finish(WEBKIT_USER_CONTENT_FILTER_STORE(store), result, &load->error);
    g_main_loop_quit(load->loop);
}
WebKitUserContentFilter *browser_blocker_load(GError **error) {
    GBytes *rules = g_resources_lookup_data("/org/jlinux/browser/blocker.json", G_RESOURCE_LOOKUP_FLAGS_NONE, error);
    if (!rules) { return NULL; }
    char *digest = g_compute_checksum_for_bytes(G_CHECKSUM_SHA256, rules);
    char *path = g_build_filename(g_get_user_cache_dir(), "jlinux", "browser", "filters", NULL);
    WebKitUserContentFilterStore *store = webkit_user_content_filter_store_new(path);
    struct filter_load load = {.loop = g_main_loop_new(NULL, FALSE)};
    webkit_user_content_filter_store_load(store, digest, NULL, loaded, &load);
    g_main_loop_run(load.loop);
    if (!load.filter) {
        g_clear_error(&load.error);
        webkit_user_content_filter_store_save(store, digest, rules, NULL, saved, &load);
        g_main_loop_run(load.loop);
    }
    if (load.error) { g_propagate_error(error, load.error); }
    g_main_loop_unref(load.loop); g_object_unref(store);
    g_free(path); g_free(digest); g_bytes_unref(rules);
    return load.filter;
}
