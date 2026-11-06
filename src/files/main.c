#define _GNU_SOURCE
#include "browser.h"
#include "../session/locale.h"
#include <glib.h>
#include <spawn.h>
#include <sys/wait.h>
#include "../effects/glass.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client.h"
extern char **environ;
enum { SIDEBAR_WIDTH = 180, CONTENT_X = 208 };
static const char *place_names[] = {"Pessoal", "Downloads", "Documentos", "Imagens", "Vídeos"};
static char *place_path(size_t index) {
    if (!index) { return g_strdup(g_get_home_dir()); }
    const GUserDirectory types[] = {G_USER_DIRECTORY_DOWNLOAD, G_USER_DIRECTORY_DOCUMENTS,
        G_USER_DIRECTORY_PICTURES, G_USER_DIRECTORY_VIDEOS};
    const char *defaults[] = {"Downloads", "Documents", "Pictures", "Videos"};
    const char *configured = g_get_user_special_dir(types[index - 1]);
    return configured ? g_strdup(configured) : g_build_filename(g_get_home_dir(), defaults[index - 1], NULL);
}
struct app;
struct buffer { struct app *app; struct wl_buffer *wl; void *data; size_t size; struct wl_list link; };
struct app {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm;
    struct wl_surface *surface;
    struct xdg_surface *xdg;
    struct xdg_toplevel *toplevel;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
    struct xkb_context *xkb;
    struct xkb_keymap *keymap;
    struct xkb_state *keys;
    struct browser browser;
    size_t selected, scroll, last_click;
    uint32_t last_click_time;
    pid_t opener;
    char message[256];
    struct wl_list buffers;
    struct jlinux_glass glass;
    int width, height, in_flight, signals;
    bool running, configured, dirty;
    double mouse_x, mouse_y;
};
static size_t rows(const struct app *app) { return app->height > 130 ? (size_t)(app->height - 130) / 38 : 1; }
static void clamp_scroll(struct app *app) {
    size_t visible = rows(app), maximum = app->browser.count > visible ? app->browser.count - visible : 0;
    if (app->scroll > maximum) { app->scroll = maximum; }
}
static bool navigate(struct app *app, const char *path) {
    if (!browser_load(&app->browser, path)) {
        snprintf(app->message, sizeof(app->message), "Não foi possível abrir a pasta: %s", strerror(errno));
        app->dirty = true; return false;
    }
    app->selected = app->scroll = 0; app->last_click = SIZE_MAX;
    app->message[0] = 0; app->dirty = true;
    if (app->toplevel) {
        char *display = g_filename_display_name(app->browser.path);
        char *title = g_strdup_printf("Arquivos — %s", display);
        xdg_toplevel_set_title(app->toplevel, title); g_free(title); g_free(display);
    }
    return true;
}
static void parent_folder(struct app *app) {
    char *parent = g_path_get_dirname(app->browser.path); navigate(app, parent); g_free(parent);
}
static void activate(struct app *app) {
    if (app->selected >= app->browser.count) { return; }
    char *path = browser_entry_path(&app->browser, app->selected);
    if (!path) { return; }
    if (app->browser.entries[app->selected].directory) { navigate(app, path); }
    else if (!app->opener) {
        char *args[] = {"gio", "open", path, NULL};
        posix_spawnattr_t attributes; int error = posix_spawnattr_init(&attributes);
        if (!error) {
            sigset_t empty; sigemptyset(&empty); posix_spawnattr_setsigmask(&attributes, &empty);
            posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK);
            pid_t pid; error = posix_spawnp(&pid, args[0], NULL, &attributes, args, environ);
            if (!error) { app->opener = pid; }
            posix_spawnattr_destroy(&attributes);
        }
        snprintf(app->message, sizeof(app->message), "%s", error ? "Não foi possível iniciar o aplicativo associado." : "Abrindo com o aplicativo associado…");
        app->dirty = true;
    }
    g_free(path);
}
static void paint(struct app *app, cairo_t *cr) {
    cairo_pattern_t *background = cairo_pattern_create_linear(0, 0, app->width, app->height);
    cairo_pattern_add_color_stop_rgb(background, 0, .095, .13, .21);
    cairo_pattern_add_color_stop_rgb(background, .55, .065, .09, .16);
    cairo_pattern_add_color_stop_rgb(background, 1, .09, .095, .18);
    cairo_set_source(cr, background); cairo_paint(cr); cairo_pattern_destroy(background);
    struct ui_rect toolbar = {12, 12, app->width - 24, 50};
    struct jlinux_glass material = app->glass; material.reflection = false;
    jlinux_glass_paint(cr, toolbar, 6, NULL, 0, 0, material);
    if (app->glass.reflection) { jlinux_glass_window_reflections(cr, toolbar, 6, 50); }
    ui_icon(cr, UI_HOME, 26, 25, 23, 0xdde6fa);
    ui_icon(cr, UI_UP, 68, 25, 23, 0xdde6fa);
    ui_icon(cr, UI_REFRESH, 110, 25, 23, 0xdde6fa);
    char *display = g_filename_display_name(app->browser.path);
    cairo_save(cr); cairo_rectangle(cr, 152, 18, app->width - 180, 38); cairo_clip(cr);
    ui_text(cr, display, 156, 27, 15, true, 0xe0e9ff); cairo_restore(cr); g_free(display);
    struct ui_rect sidebar = {12, 74, SIDEBAR_WIDTH, app->height - 112};
    jlinux_glass_paint(cr, sidebar, 6, NULL, 0, 0, material);
    ui_text(cr, "LOCAIS", 26, 84, 11, true, 0xa9b7d0);
    for (size_t i = 0; i < 5; i++) {
        double y = 108 + i * 40;
        char *path = place_path(i);
        char *canonical = realpath(path, NULL);
        bool active = canonical && !strcmp(canonical, app->browser.path);
        bool available = g_file_test(path, G_FILE_TEST_IS_DIR);
        if (active) {
            cairo_set_source_rgba(cr, .35, .47, .78, .30);
            cairo_rectangle(cr, 20, y, SIDEBAR_WIDTH - 16, 36); cairo_fill(cr);
        }
        ui_icon(cr, i ? UI_FOLDER : UI_HOME, 28, y + 8, 20, available ? 0x94bafa : 0x76849e);
        ui_text(cr, place_names[i], 58, y + 10, 13, active, available ? 0xe0e6f5 : 0x94a0b7);
        free(canonical); g_free(path);
    }
    bool details = app->width >= 760;
    ui_text(cr, "Nome", CONTENT_X + 34, 77, 13, true, 0x9eadc7);
    if (details) { ui_text(cr, "Tipo / tamanho", app->width - 172, 77, 13, true, 0x9eadc7); }
    size_t visible = rows(app);
    for (size_t row = 0; row < visible && row + app->scroll < app->browser.count; row++) {
        size_t index = row + app->scroll; double y = 100 + row * 38;
        struct file_entry *entry = &app->browser.entries[index];
        if (index == app->selected) {
            cairo_set_source_rgba(cr, .26, .34, .58, .34); cairo_rectangle(cr, CONTENT_X, y, app->width - CONTENT_X - 14, 36); cairo_fill(cr);
        }
        ui_icon(cr, entry->directory ? UI_FOLDER : UI_FILE, CONTENT_X + 11, y + 7, 22, entry->directory ? 0x87b8f5 : 0xa9b7d0);
        cairo_save(cr); cairo_rectangle(cr, CONTENT_X + 44, y + 2, app->width - CONTENT_X - 60 - (details ? 180 : 0), 34); cairo_clip(cr);
        ui_text(cr, entry->label, CONTENT_X + 44, y + 9, 14, false, 0xe0e6f5); cairo_restore(cr);
        char detail[64];
        if (entry->directory) { snprintf(detail, sizeof(detail), "%s", entry->symlink ? "Pasta (atalho)" : "Pasta"); }
        else if (entry->symlink) { snprintf(detail, sizeof(detail), "Atalho"); }
        else if (entry->size >= 1048576) { snprintf(detail, sizeof(detail), "%.1f MiB", entry->size / 1048576.0); }
        else if (entry->size >= 1024) { snprintf(detail, sizeof(detail), "%.1f KiB", entry->size / 1024.0); }
        else { snprintf(detail, sizeof(detail), "%lld B", (long long)entry->size); }
        if (details) { ui_text(cr, detail, app->width - 172, y + 9, 12, false, 0x9eadc7); }
    }
    if (!app->browser.count) { ui_text(cr, "Esta pasta está vazia.", CONTENT_X + 12, 117, 15, false, 0xa9b7d0); }
    char status[128]; snprintf(status, sizeof(status), "%zu itens • F5 atualizar • Ctrl+H ocultos", app->browser.count);
    cairo_save(cr); cairo_rectangle(cr, 16, app->height - 26, app->width - 32, 25); cairo_clip(cr);
    ui_text(cr, app->message[0] ? app->message : status, 20, app->height - 23, 12, false, 0xafbdd6); cairo_restore(cr);
}
static void destroy_buffer(struct buffer *buffer) {
    wl_buffer_destroy(buffer->wl); munmap(buffer->data, buffer->size);
    wl_list_remove(&buffer->link); buffer->app->in_flight--; free(buffer);
}
static void released(void *data, struct wl_buffer *wl) { (void)wl; destroy_buffer(data); }
static const struct wl_buffer_listener buffer_listener = {.release = released};
static bool draw(struct app *app) {
    if (!app->configured || !app->dirty || app->in_flight >= 2) { return true; }
    if (app->width < 540 || app->height < 360 || app->width > 8192 || app->height > 8192 || (size_t)app->width * app->height > 16777216) { return false; }
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, app->width);
    size_t size = (size_t)stride * app->height;
    struct buffer *buffer = calloc(1, sizeof(*buffer));
    if (!buffer) { return false; }
    int fd = memfd_create("jlinux-login", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, (off_t)size)) { if (fd >= 0) { close(fd); } free(buffer); return false; }
    buffer->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer->data == MAP_FAILED) { close(fd); free(buffer); return false; }
    struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, (int)size);
    if (!pool) { close(fd); munmap(buffer->data, size); free(buffer); return false; }
    buffer->wl = wl_shm_pool_create_buffer(pool, 0, app->width, app->height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool); close(fd);
    if (!buffer->wl) { munmap(buffer->data, size); free(buffer); return false; }
    buffer->app = app; buffer->size = size;
    wl_list_insert(&app->buffers, &buffer->link); app->in_flight++;
    wl_buffer_add_listener(buffer->wl, &buffer_listener, buffer);
    cairo_surface_t *image = cairo_image_surface_create_for_data(buffer->data, CAIRO_FORMAT_ARGB32, app->width, app->height, stride);
    cairo_t *cr = cairo_create(image); paint(app, cr);
    bool valid = cairo_status(cr) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr); cairo_surface_destroy(image);
    if (!valid) { destroy_buffer(buffer); return false; }
    wl_surface_attach(app->surface, buffer->wl, 0, 0);
    wl_surface_damage_buffer(app->surface, 0, 0, app->width, app->height); wl_surface_commit(app->surface);
    app->dirty = false; return true;
}
static void keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size) {
    (void)keyboard; struct app *app = data;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size < 1 || size > 16777216) { close(fd); app->running = false; return; }
    char *mapping = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0); close(fd);
    if (mapping == MAP_FAILED) { app->running = false; return; }
    struct xkb_keymap *map = mapping[size - 1] == 0 ? xkb_keymap_new_from_string(app->xkb, mapping, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    munmap(mapping, size);
    struct xkb_state *state = map ? xkb_state_new(map) : NULL;
    if (!state) { xkb_keymap_unref(map); app->running = false; return; }
    xkb_state_unref(app->keys); xkb_keymap_unref(app->keymap); app->keymap = map; app->keys = state;
}
static void keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) { (void)data; (void)keyboard; (void)serial; (void)surface; (void)keys; }
static void keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {
    (void)data; (void)keyboard; (void)serial; (void)surface;
}
static void key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t code, uint32_t state) {
    (void)keyboard; (void)serial; (void)time; struct app *app = data;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !app->keys) { return; }
    xkb_keysym_t symbol = xkb_state_key_get_one_sym(app->keys, code + 8);
    if (symbol >= XKB_KEY_1 && symbol <= XKB_KEY_5 && xkb_state_mod_name_is_active(app->keys, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) {
        char *path = place_path(symbol - XKB_KEY_1); navigate(app, path); g_free(path);
    } else if (symbol == XKB_KEY_Return || symbol == XKB_KEY_KP_Enter) { activate(app); }
    else if (symbol == XKB_KEY_BackSpace || (symbol == XKB_KEY_Up && xkb_state_mod_name_is_active(app->keys, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)) { parent_folder(app); }
    else if (symbol == XKB_KEY_F5) { navigate(app, app->browser.path); }
    else if ((symbol == XKB_KEY_h || symbol == XKB_KEY_H) && xkb_state_mod_name_is_active(app->keys, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) {
        app->browser.hidden = !app->browser.hidden; if (!navigate(app, app->browser.path)) { app->browser.hidden = !app->browser.hidden; }
    } else if (symbol == XKB_KEY_Down && app->selected + 1 < app->browser.count) { app->selected++; }
    else if (symbol == XKB_KEY_Up && app->selected) { app->selected--; }
    if (app->selected < app->scroll) { app->scroll = app->selected; }
    if (app->selected >= app->scroll + rows(app)) { app->scroll = app->selected - rows(app) + 1; }
    clamp_scroll(app); app->dirty = true;
}
static void modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    (void)keyboard; (void)serial; struct app *app = data;
    if (app->keys) { xkb_state_update_mask(app->keys, depressed, latched, locked, 0, 0, group); }
}
static const struct wl_keyboard_listener keyboard_listener = {.keymap = keymap, .enter = keyboard_enter, .leave = keyboard_leave, .key = key, .modifiers = modifiers};
static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    (void)surface; struct app *app = data; app->mouse_x = wl_fixed_to_double(x); app->mouse_y = wl_fixed_to_double(y);
    struct wl_cursor *cursor = app->cursor_theme ? wl_cursor_theme_get_cursor(app->cursor_theme, "left_ptr") : NULL;
    if (cursor && cursor->image_count) {
        struct wl_cursor_image *image = cursor->images[0]; wl_pointer_set_cursor(pointer, serial, app->cursor_surface, image->hotspot_x, image->hotspot_y);
        wl_surface_attach(app->cursor_surface, wl_cursor_image_get_buffer(image), 0, 0); wl_surface_damage(app->cursor_surface, 0, 0, image->width, image->height); wl_surface_commit(app->cursor_surface);
    }
}
static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) { (void)data; (void)pointer; (void)serial; (void)surface; }
static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) { (void)pointer; (void)time; struct app *app = data; app->mouse_x = wl_fixed_to_double(x); app->mouse_y = wl_fixed_to_double(y); }
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void)pointer; (void)serial; struct app *app = data;
    if (button != 272 || state != WL_POINTER_BUTTON_STATE_RELEASED) { return; }
    if (app->mouse_y >= 12 && app->mouse_y < 62) {
        if (app->mouse_x >= 20 && app->mouse_x < 60) { navigate(app, g_get_home_dir()); }
        else if (app->mouse_x >= 62 && app->mouse_x < 102) { parent_folder(app); }
        else if (app->mouse_x >= 104 && app->mouse_x < 144) { navigate(app, app->browser.path); }
    } else if (app->mouse_x >= 20 && app->mouse_x < 12 + SIDEBAR_WIDTH && app->mouse_y >= 108 && app->mouse_y < 308) {
        size_t index = (size_t)((app->mouse_y - 108) / 40);
        char *path = place_path(index); navigate(app, path); g_free(path);
    } else if (app->mouse_x >= CONTENT_X && app->mouse_y >= 100 && app->mouse_y < app->height - 30) {
        size_t index = app->scroll + (size_t)((app->mouse_y - 100) / 38);
        if (index < app->browser.count) {
            app->selected = index;
            if (app->last_click == index && (uint32_t)(time - app->last_click_time) < 400) { activate(app); app->last_click = SIZE_MAX; }
            else { app->last_click = index; app->last_click_time = time; }
        }
    }
    app->dirty = true;
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)pointer; (void)time; struct app *app = data;
    if (app->mouse_x < CONTENT_X || axis != WL_POINTER_AXIS_VERTICAL_SCROLL) { return; }
    if (value > 0) { app->scroll += 3; } else { app->scroll = app->scroll > 3 ? app->scroll - 3 : 0; }
    clamp_scroll(app); app->dirty = true;
}
static const struct wl_pointer_listener pointer_listener = {.enter = pointer_enter, .leave = pointer_leave, .motion = pointer_motion, .button = pointer_button, .axis = pointer_axis};
static void capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct app *app = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) { app->keyboard = wl_seat_get_keyboard(seat); wl_keyboard_add_listener(app->keyboard, &keyboard_listener, app); }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) { app->pointer = wl_seat_get_pointer(seat); wl_pointer_add_listener(app->pointer, &pointer_listener, app); }
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && app->keyboard) { wl_keyboard_destroy(app->keyboard); app->keyboard = NULL; }
    if (!(caps & WL_SEAT_CAPABILITY_POINTER) && app->pointer) { wl_pointer_destroy(app->pointer); app->pointer = NULL; }
}
static const struct wl_seat_listener seat_listener = {.capabilities = capabilities};
static void ping(void *data, struct xdg_wm_base *wm, uint32_t serial) { (void)data; xdg_wm_base_pong(wm, serial); }
static const struct xdg_wm_base_listener wm_listener = {.ping = ping};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct app *app = data;
    if (!strcmp(interface, wl_compositor_interface.name) && version >= 4) { app->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4); }
    else if (!strcmp(interface, wl_shm_interface.name)) { app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1); }
    else if (!strcmp(interface, xdg_wm_base_interface.name)) { app->wm = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1); xdg_wm_base_add_listener(app->wm, &wm_listener, app); }
    else if (!strcmp(interface, wl_seat_interface.name)) { app->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1); wl_seat_add_listener(app->seat, &seat_listener, app); }
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = removed};
static void configured(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct app *app = data; xdg_surface_ack_configure(surface, serial);
    struct wl_region *opaque = wl_compositor_create_region(app->compositor);
    if (!opaque) { app->running = false; return; }
    wl_region_add(opaque, 0, 0, app->width, app->height);
    wl_surface_set_opaque_region(app->surface, opaque); wl_region_destroy(opaque);
    app->configured = true; app->dirty = true;
}
static const struct xdg_surface_listener surface_listener = {.configure = configured};
static void resized(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states) { (void)toplevel; (void)states; struct app *app = data; if (width > 0 && height > 0) { app->width = width < 540 ? 540 : width; app->height = height < 360 ? 360 : height; clamp_scroll(app); } }
static void closed(void *data, struct xdg_toplevel *toplevel) { (void)toplevel; struct app *app = data; app->running = false; }
static const struct xdg_toplevel_listener toplevel_listener = {.configure = resized, .close = closed};
static int preview(const char *path, const char *directory) {
    struct app app = {.width = 1000, .height = 640};
    if (!jlinux_glass_settings(&app.glass) || !navigate(&app, directory)) { return 1; }
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, app.width, app.height); cairo_t *cr = cairo_create(image);
    paint(&app, cr); cairo_status_t result = cairo_surface_write_to_png(image, path);
    cairo_destroy(cr); cairo_surface_destroy(image); browser_finish(&app.browser); ui_finish();
    return result == CAIRO_STATUS_SUCCESS ? 0 : 1;
}
int main(int argc, char **argv) {
    if (!jlinux_session_locale("/etc/locale.conf")) { return 1; }
    setlocale(LC_ALL, "");
    if (argc == 4 && !strcmp(argv[1], "--preview")) { return preview(argv[2], argv[3]); }
    if (argc > 2) { fprintf(stderr, "Uso: jlinux-files [pasta]\n"); return 1; }
    struct app app = {.running = true, .dirty = true, .width = 1000, .height = 640, .signals = -1, .last_click = SIZE_MAX};
    wl_list_init(&app.buffers);
    if (!jlinux_glass_settings(&app.glass) || !navigate(&app, argc == 2 ? argv[1] : g_get_home_dir())) {
        fprintf(stderr, "Não foi possível abrir a pasta inicial.\n"); browser_finish(&app.browser); return 1;
    }
    sigset_t signals; sigemptyset(&signals); sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM); sigaddset(&signals, SIGCHLD); sigprocmask(SIG_BLOCK, &signals, NULL);
    app.signals = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    app.display = wl_display_connect(NULL); int result = 1;
    if (!app.display || app.signals < 0) { goto cleanup; }
    app.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!app.xkb) { goto cleanup; }
    app.registry = wl_display_get_registry(app.display); wl_registry_add_listener(app.registry, &registry_listener, &app);
    if (wl_display_roundtrip(app.display) < 0 || !app.compositor || !app.shm || !app.wm) { goto cleanup; }
    app.cursor_theme = wl_cursor_theme_load(NULL, 24, app.shm); app.cursor_surface = wl_compositor_create_surface(app.compositor);
    app.surface = wl_compositor_create_surface(app.compositor);
    if (!app.surface || !app.cursor_surface) { goto cleanup; }
    app.xdg = xdg_wm_base_get_xdg_surface(app.wm, app.surface); xdg_surface_add_listener(app.xdg, &surface_listener, &app);
    app.toplevel = xdg_surface_get_toplevel(app.xdg); xdg_toplevel_add_listener(app.toplevel, &toplevel_listener, &app);
    xdg_toplevel_set_title(app.toplevel, "Arquivos"); xdg_toplevel_set_app_id(app.toplevel, "org.jlinux.Files"); xdg_toplevel_set_min_size(app.toplevel, 540, 360);
    wl_surface_commit(app.surface);
    while (app.running) {
        if (wl_display_dispatch_pending(app.display) < 0 || !draw(&app)) { goto cleanup; }
        while (wl_display_prepare_read(app.display)) { if (wl_display_dispatch_pending(app.display) < 0) { goto cleanup; } }
        int flushed = wl_display_flush(app.display);
        if (flushed < 0 && errno != EAGAIN) { wl_display_cancel_read(app.display); goto cleanup; }
        struct pollfd fds[] = {{wl_display_get_fd(app.display), POLLIN | (flushed < 0 ? POLLOUT : 0), 0}, {app.signals, POLLIN, 0}};
        int ready = poll(fds, 2, -1);
        if (ready < 0) { wl_display_cancel_read(app.display); if (errno == EINTR) { continue; } goto cleanup; }
        if (fds[0].revents & POLLIN) { if (wl_display_read_events(app.display) < 0) { goto cleanup; } }
        else { wl_display_cancel_read(app.display); }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) { goto cleanup; }
        if (fds[1].revents & POLLIN) {
            struct signalfd_siginfo event;
            while (read(app.signals, &event, sizeof(event)) == sizeof(event)) {
                if (event.ssi_signo != SIGCHLD) { app.running = false; continue; }
                int status; pid_t pid;
                while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
                    if (pid == app.opener) {
                        app.opener = 0;
                        snprintf(app.message, sizeof(app.message), "%s", WIFEXITED(status) && WEXITSTATUS(status) == 0 ? "" : "Não foi possível abrir. Confira o aplicativo associado e as permissões.");
                        app.dirty = true;
                    }
                }
            }
        }
    }
    result = 0;
cleanup:
    if (app.pointer) { wl_pointer_destroy(app.pointer); } if (app.keyboard) { wl_keyboard_destroy(app.keyboard); }
    if (app.seat) { wl_seat_destroy(app.seat); }
    if (app.toplevel) { xdg_toplevel_destroy(app.toplevel); } if (app.xdg) { xdg_surface_destroy(app.xdg); }
    if (app.surface) { wl_surface_destroy(app.surface); } if (app.cursor_surface) { wl_surface_destroy(app.cursor_surface); }
    if (app.cursor_theme) { wl_cursor_theme_destroy(app.cursor_theme); }
    struct buffer *buffer, *next; wl_list_for_each_safe(buffer, next, &app.buffers, link) { destroy_buffer(buffer); }
    if (app.wm) { xdg_wm_base_destroy(app.wm); } if (app.shm) { wl_shm_destroy(app.shm); }
    if (app.compositor) { wl_compositor_destroy(app.compositor); } if (app.registry) { wl_registry_destroy(app.registry); }
    if (app.display) { wl_display_flush(app.display); wl_display_disconnect(app.display); }
    if (app.signals >= 0) { close(app.signals); }
    xkb_state_unref(app.keys); xkb_keymap_unref(app.keymap); xkb_context_unref(app.xkb);
    browser_finish(&app.browser);
    ui_finish(); return result;
}
