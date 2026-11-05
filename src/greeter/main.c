#define _GNU_SOURCE
#include "auth.h"
#include "../effects/glass.h"
#include "../renderer/blur.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include "xdg-shell-client.h"
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
    struct xkb_compose_table *compose_table;
    struct xkb_compose_state *compose;
    struct login_auth auth;
    struct wl_list buffers;
    struct jlinux_glass glass;
    cairo_surface_t *wallpaper, *backdrop;
    int width, height, cached_width, cached_height, in_flight, signals;
    bool running, configured, dirty, demo, fatal;
    double mouse_x, mouse_y;
    char username[256], input[1024];
};
static struct ui_rect card(const struct app *app) { return (struct ui_rect){(app->width - 448) / 2.0, (app->height - 372) / 2.0, 448, 372}; }
static bool prepare_background(struct app *app) {
    if (app->wallpaper && app->cached_width == app->width && app->cached_height == app->height) { return true; }
    if (app->wallpaper) { cairo_surface_destroy(app->wallpaper); }
    if (app->backdrop) { cairo_surface_destroy(app->backdrop); }
    app->wallpaper = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, app->width, app->height);
    cairo_t *cr = cairo_create(app->wallpaper); ui_wallpaper(cr, app->width, app->height); cairo_destroy(cr);
    app->backdrop = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (app->width + 3) / 4, (app->height + 3) / 4);
    cr = cairo_create(app->backdrop); cairo_scale(cr, .25, .25); ui_wallpaper(cr, app->width, app->height); cairo_destroy(cr);
    app->cached_width = app->width; app->cached_height = app->height;
    return cairo_surface_status(app->wallpaper) == CAIRO_STATUS_SUCCESS &&
        cairo_surface_status(app->backdrop) == CAIRO_STATUS_SUCCESS && (!app->glass.blur || jlinux_blur(app->backdrop, 5));
}
static void paint(struct app *app, cairo_t *cr) {
    cairo_set_source_surface(cr, app->wallpaper, 0, 0); cairo_paint(cr);
    cairo_set_source_rgba(cr, .005, .01, .025, .25); cairo_paint(cr);
    struct ui_rect r = card(app);
    cairo_save(cr); cairo_translate(cr, r.x, r.y);
    jlinux_glass_paint(cr, (struct ui_rect){.5, .5, r.width - 1, r.height - 1}, 12,
        app->backdrop, r.x, r.y, app->glass);
    if (app->glass.shadow) { jlinux_glass_window_shading(cr, (struct ui_rect){.5, .5, r.width - 1, r.height - 1}, 12); }
    if (app->glass.reflection) { jlinux_glass_window_reflections(cr, (struct ui_rect){.5, .5, r.width - 1, r.height - 1}, 12, 60); }
    ui_text(cr, "J", 30, 22, 35, true, 0xb69aff); ui_text(cr, "JLinux", 62, 29, 26, true, 0xf1f3ff);
    ui_text(cr, "Seu espaço. Do seu jeito.", 30, 83, 16, false, 0xd4dcef);
    bool prompt = app->demo || app->auth.state == LOGIN_PROMPT;
    ui_text(cr, prompt ? (app->auth.secret || app->demo ? "Senha" : "Resposta de autenticação") : "Usuário",
        30, 137, 14, true, 0xdde5fa);
    cairo_set_source_rgba(cr, .015, .025, .05, .48); cairo_rectangle(cr, 30, 166, 388, 48); cairo_fill(cr);
    cairo_set_source_rgba(cr, .52, .63, .91, .75); cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, 30.5, 166.5, 387, 47); cairo_stroke(cr);
    char shown[256];
    if (prompt && (app->auth.secret || app->demo)) {
        size_t count = 0;
        for (size_t i = 0; app->input[i]; i++) { if (((unsigned char)app->input[i] & 0xc0) != 0x80) { count++; } }
        if (count > 48) { count = 48; }
        memset(shown, '*', count); shown[count] = 0;
    } else { snprintf(shown, sizeof(shown), "%.255s", prompt ? app->input : app->username); }
    cairo_save(cr); cairo_rectangle(cr, 40, 171, 366, 36); cairo_clip(cr);
    ui_text(cr, shown[0] ? shown : (prompt ? "Digite sua senha" : "Seu nome de usuário"), 43, 177, 17, false, 0xeaf0ff);
    cairo_restore(cr); explicit_bzero(shown, sizeof(shown));
    cairo_set_source_rgba(cr, .29, .34, .57, .65); cairo_rectangle(cr, 30, 236, 388, 44); cairo_fill(cr);
    const char *button = app->fatal ? "Tentar novamente" : app->auth.state == LOGIN_WAIT || app->auth.state == LOGIN_STARTING ? "Aguarde…" : "Entrar no JLinux";
    ui_text(cr, button, 146, 247, 15, true, 0xf0f3ff);
    cairo_save(cr); cairo_rectangle(cr, 30, 294, 388, 52); cairo_clip(cr);
    const char *message = app->demo ? "Prévia visual — autenticação desativada" : app->auth.message[0] ? app->auth.message : "Enter para continuar • Esc para recomeçar";
    ui_text(cr, message, 30, 300, 12, false, 0xc5cde2); cairo_restore(cr);
    cairo_restore(cr);
    ui_text(cr, "JLinux • Sessão Wayland", 24, app->height - 36, 13, false, 0xa9b7d0);
}
static void destroy_buffer(struct buffer *buffer) {
    wl_buffer_destroy(buffer->wl); munmap(buffer->data, buffer->size);
    wl_list_remove(&buffer->link); buffer->app->in_flight--; free(buffer);
}
static void released(void *data, struct wl_buffer *wl) { (void)wl; destroy_buffer(data); }
static const struct wl_buffer_listener buffer_listener = {.release = released};
static bool draw(struct app *app) {
    if (!app->configured || !app->dirty || app->in_flight >= 2) { return true; }
    if (app->width < 480 || app->height < 420 || app->width > 8192 || app->height > 8192 || (size_t)app->width * app->height > 16777216) { return false; }
    if (!prepare_background(app)) { return false; }
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
static void reset_auth(struct app *app) {
    explicit_bzero(app->input, sizeof(app->input));
    if (app->demo) { return; }
    if (app->fatal) { app->running = false; return; }
    if (app->auth.state != LOGIN_READY && !app->auth.tx_size) { login_auth_cancel(&app->auth); }
}
static void submit(struct app *app) {
    if (app->demo) { snprintf(app->auth.message, sizeof(app->auth.message), "Prévia sem autenticação"); return; }
    if (app->fatal) { app->running = false; return; }
    bool valid = true;
    if (app->auth.state == LOGIN_READY) { valid = login_auth_create(&app->auth, app->username); }
    else if (app->auth.state == LOGIN_PROMPT) { valid = login_auth_reply(&app->auth, app->input); }
    if (!valid && app->auth.state == LOGIN_FAILED) { app->fatal = true; }
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
    (void)keyboard; (void)serial; (void)surface; struct app *app = data;
    explicit_bzero(app->input, sizeof(app->input)); if (app->compose) { xkb_compose_state_reset(app->compose); } app->dirty = true;
}
static void key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t code, uint32_t state) {
    (void)keyboard; (void)serial; (void)time; struct app *app = data;
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !app->keys) { return; }
    xkb_keysym_t symbol = xkb_state_key_get_one_sym(app->keys, code + 8);
    app->dirty = true;
    if (symbol == XKB_KEY_Return || symbol == XKB_KEY_KP_Enter) { submit(app); return; }
    if (symbol == XKB_KEY_Escape) { reset_auth(app); return; }
    char *target = app->demo || app->auth.state == LOGIN_PROMPT ? app->input : app->auth.state == LOGIN_READY ? app->username : NULL;
    size_t capacity = target == app->username ? sizeof(app->username) : sizeof(app->input);
    if (!target || app->fatal) { return; }
    size_t length = strlen(target);
    if (symbol == XKB_KEY_BackSpace) {
        if (app->compose) { xkb_compose_state_reset(app->compose); }
        if (length) { size_t end = length; do { length--; } while (length && ((unsigned char)target[length] & 0xc0) == 0x80); explicit_bzero(target + length, end - length); }
        return;
    }
    if (xkb_state_mod_name_is_active(app->keys, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0 ||
        xkb_state_mod_name_is_active(app->keys, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) { return; }
    char text[64]; int count = 0;
    if (app->compose && xkb_compose_state_feed(app->compose, symbol) == XKB_COMPOSE_FEED_ACCEPTED) {
        enum xkb_compose_status status = xkb_compose_state_get_status(app->compose);
        if (status == XKB_COMPOSE_COMPOSING) { return; }
        if (status == XKB_COMPOSE_COMPOSED) { count = xkb_compose_state_get_utf8(app->compose, text, sizeof(text)); xkb_compose_state_reset(app->compose); }
        else if (status == XKB_COMPOSE_CANCELLED) { xkb_compose_state_reset(app->compose); return; }
        else { count = xkb_state_key_get_utf8(app->keys, code + 8, text, sizeof(text)); }
    } else { count = xkb_state_key_get_utf8(app->keys, code + 8, text, sizeof(text)); }
    if (count > 0 && (size_t)count < sizeof(text) && (unsigned char)text[0] >= 32 && length + (size_t)count < capacity) {
        memcpy(target + length, text, (size_t)count); target[length + (size_t)count] = 0;
    }
    explicit_bzero(text, sizeof(text));
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
    (void)pointer; (void)serial; (void)time; struct app *app = data; struct ui_rect r = card(app);
    if (button == 272 && state == WL_POINTER_BUTTON_STATE_RELEASED && ui_contains((struct ui_rect){r.x + 30, r.y + 236, 388, 44}, app->mouse_x, app->mouse_y)) { submit(app); app->dirty = true; }
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) { (void)data; (void)pointer; (void)time; (void)axis; (void)value; }
static const struct wl_pointer_listener pointer_listener = {.enter = pointer_enter, .leave = pointer_leave, .motion = pointer_motion, .button = pointer_button, .axis = pointer_axis};
static void capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    struct app *app = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) { app->keyboard = wl_seat_get_keyboard(seat); wl_keyboard_add_listener(app->keyboard, &keyboard_listener, app); }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) { app->pointer = wl_seat_get_pointer(seat); wl_pointer_add_listener(app->pointer, &pointer_listener, app); }
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && app->keyboard) { wl_keyboard_destroy(app->keyboard); app->keyboard = NULL; explicit_bzero(app->input, sizeof(app->input)); }
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
static void configured(void *data, struct xdg_surface *surface, uint32_t serial) { struct app *app = data; xdg_surface_ack_configure(surface, serial); app->configured = true; app->dirty = true; }
static const struct xdg_surface_listener surface_listener = {.configure = configured};
static void resized(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states) { (void)toplevel; (void)states; struct app *app = data; if (width > 0 && height > 0) { app->width = width; app->height = height; } }
static void closed(void *data, struct xdg_toplevel *toplevel) { (void)toplevel; struct app *app = data; app->running = false; }
static const struct xdg_toplevel_listener toplevel_listener = {.configure = resized, .close = closed};
static int preview(const char *path) {
    struct app app = {.width = 1600, .height = 900, .demo = true};
    if (!jlinux_glass_settings(&app.glass) || !prepare_background(&app)) { return 1; }
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, app.width, app.height); cairo_t *cr = cairo_create(image);
    paint(&app, cr); cairo_status_t result = cairo_surface_write_to_png(image, path);
    cairo_destroy(cr); cairo_surface_destroy(image); cairo_surface_destroy(app.wallpaper); cairo_surface_destroy(app.backdrop); ui_finish();
    return result == CAIRO_STATUS_SUCCESS ? 0 : 1;
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--preview")) { return preview(argv[2]); }
    bool demo = argc == 2 && !strcmp(argv[1], "--demo");
    if (argc != 1 && !demo) { fprintf(stderr, "Uso: jlinux-greeter [--demo|--preview arquivo.png]\n"); return 1; }
    struct app app = {.running = true, .dirty = true, .demo = demo, .width = 1280, .height = 720, .signals = -1, .auth = {.fd = -1}};
    wl_list_init(&app.buffers); prctl(PR_SET_DUMPABLE, 0); mlock(app.input, sizeof(app.input)); mlock(app.auth.tx, sizeof(app.auth.tx));
    if (!jlinux_glass_settings(&app.glass)) { return 1; }
    if (!demo && !login_auth_open(&app.auth, getenv("GREETD_SOCK"))) { fprintf(stderr, "Serviço greetd indisponível.\n"); return 1; }
    sigset_t signals; sigemptyset(&signals); sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM); sigprocmask(SIG_BLOCK, &signals, NULL);
    app.signals = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    app.display = wl_display_connect(NULL); int result = 1;
    if (!app.display || app.signals < 0) { goto cleanup; }
    const char *locale = setlocale(LC_CTYPE, ""); app.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!app.xkb) { goto cleanup; }
    app.compose_table = xkb_compose_table_new_from_locale(app.xkb, locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (app.compose_table) { app.compose = xkb_compose_state_new(app.compose_table, XKB_COMPOSE_STATE_NO_FLAGS); }
    app.registry = wl_display_get_registry(app.display); wl_registry_add_listener(app.registry, &registry_listener, &app);
    if (wl_display_roundtrip(app.display) < 0 || !app.compositor || !app.shm || !app.wm) { goto cleanup; }
    app.cursor_theme = wl_cursor_theme_load(NULL, 24, app.shm); app.cursor_surface = wl_compositor_create_surface(app.compositor);
    app.surface = wl_compositor_create_surface(app.compositor);
    if (!app.surface || !app.cursor_surface) { goto cleanup; }
    app.xdg = xdg_wm_base_get_xdg_surface(app.wm, app.surface); xdg_surface_add_listener(app.xdg, &surface_listener, &app);
    app.toplevel = xdg_surface_get_toplevel(app.xdg); xdg_toplevel_add_listener(app.toplevel, &toplevel_listener, &app);
    xdg_toplevel_set_title(app.toplevel, "JLinux Login"); xdg_toplevel_set_app_id(app.toplevel, "org.jlinux.Login"); xdg_toplevel_set_fullscreen(app.toplevel, NULL);
    wl_surface_commit(app.surface);
    while (app.running) {
        if (wl_display_dispatch_pending(app.display) < 0 || !draw(&app)) { goto cleanup; }
        while (wl_display_prepare_read(app.display)) { if (wl_display_dispatch_pending(app.display) < 0) { goto cleanup; } }
        int flushed = wl_display_flush(app.display);
        if (flushed < 0 && errno != EAGAIN) { wl_display_cancel_read(app.display); goto cleanup; }
        struct pollfd fds[] = {{wl_display_get_fd(app.display), POLLIN | (flushed < 0 ? POLLOUT : 0), 0}, {app.signals, POLLIN, 0},
            {app.auth.fd, POLLIN | ((!app.auth.connected || app.auth.tx_size) ? POLLOUT : 0), 0}};
        int ready = poll(fds, 3, -1);
        if (ready < 0) { wl_display_cancel_read(app.display); if (errno == EINTR) { continue; } goto cleanup; }
        if (fds[0].revents & POLLIN) { if (wl_display_read_events(app.display) < 0) { goto cleanup; } }
        else { wl_display_cancel_read(app.display); }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) { goto cleanup; }
        if (fds[1].revents & POLLIN) { app.running = false; }
        if (fds[2].revents) {
            if (!login_auth_dispatch(&app.auth, fds[2].revents)) { app.fatal = true; login_auth_close(&app.auth); explicit_bzero(app.input, sizeof(app.input)); }
            if (app.auth.state == LOGIN_DONE) { result = 0; goto cleanup; }
            app.dirty = true;
        }
    }
    result = demo ? 0 : 1;
cleanup:
    explicit_bzero(app.input, sizeof(app.input)); login_auth_close(&app.auth); munlock(app.input, sizeof(app.input)); munlock(app.auth.tx, sizeof(app.auth.tx));
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
    xkb_compose_state_unref(app.compose); xkb_compose_table_unref(app.compose_table);
    xkb_state_unref(app.keys); xkb_keymap_unref(app.keymap); xkb_context_unref(app.xkb);
    if (app.wallpaper) { cairo_surface_destroy(app.wallpaper); } if (app.backdrop) { cairo_surface_destroy(app.backdrop); }
    ui_finish(); return result;
}
