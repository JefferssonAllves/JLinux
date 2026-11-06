#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client.h"

/* Protocol fixtures for constraints and transient windows, outside the desktop runtime. */
struct window {
    int id, width, height;
    struct wl_surface *surface;
    struct xdg_surface *xdg;
    struct xdg_toplevel *top;
};
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct xdg_wm_base *shell;
static struct wl_buffer *buffer;
static struct window windows[8];
static void ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    (void)data; xdg_wm_base_pong(base, serial);
}
static const struct xdg_wm_base_listener shell_listener = {.ping = ping};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)data; (void)version;
    if (!strcmp(interface, "wl_compositor")) { compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4); }
    if (!strcmp(interface, "wl_shm")) { shm = wl_registry_bind(registry, name, &wl_shm_interface, 1); }
    if (!strcmp(interface, "xdg_wm_base")) {
        shell = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(shell, &shell_listener, NULL);
    }
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = removed};
static void configured(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct window *window = data;
    xdg_surface_ack_configure(surface, serial);
    int width = window->width > 0 ? window->width : 400;
    int height = window->height > 0 ? window->height : 300;
    assert(width <= 1280 && height <= 720);
    xdg_surface_set_window_geometry(surface, 0, 0, width, height);
    wl_surface_attach(window->surface, buffer, 0, 0);
    wl_surface_damage(window->surface, 0, 0, 1280, 720);
    wl_surface_commit(window->surface);
    printf("size %d %d %d\n", window->id, width, height); fflush(stdout);
}
static const struct xdg_surface_listener surface_listener = {.configure = configured};
static void sized(void *data, struct xdg_toplevel *top, int32_t width, int32_t height, struct wl_array *states) {
    (void)top;
    struct window *window = data;
    window->width = width; window->height = height;
    bool active = false; uint32_t *state;
    wl_array_for_each(state, states) { active |= *state == XDG_TOPLEVEL_STATE_ACTIVATED; }
    printf("active %d %d\n", window->id, active); fflush(stdout);
}
static void closed(void *data, struct xdg_toplevel *top) { (void)data; (void)top; }
static const struct xdg_toplevel_listener top_listener = {.configure = sized, .close = closed};
int main(void) {
    struct wl_display *display = wl_display_connect(NULL);
    assert(display);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    assert(wl_display_roundtrip(display) >= 0 && compositor && shm && shell);
    char path[] = "/tmp/jlinux-layout-buffer-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0); unlink(path);
    const int bytes = 1280 * 720 * 4;
    assert(ftruncate(fd, bytes) == 0);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, bytes);
    buffer = wl_shm_pool_create_buffer(pool, 0, 1280, 720, 1280 * 4, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool); close(fd);
    while (true) {
        assert(wl_display_dispatch_pending(display) >= 0);
        assert(wl_display_flush(display) >= 0);
        struct pollfd fds[] = {{wl_display_get_fd(display), POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}};
        assert(poll(fds, 2, -1) >= 0);
        if (fds[0].revents & POLLIN) { assert(wl_display_dispatch(display) >= 0); }
        if (fds[1].revents & (POLLIN | POLLHUP)) {
            char line[128], command[16]; int id, parent, minw, minh, maxw, maxh;
            if (!fgets(line, sizeof(line), stdin)) { break; }
            assert(sscanf(line, "%15s %d %d %d %d %d %d", command, &id, &parent, &minw, &minh, &maxw, &maxh) == 7);
            assert(id >= 0 && id < 8);
            struct window *window = &windows[id];
            if (!strcmp(command, "create")) {
                window->id = id;
                window->surface = wl_compositor_create_surface(compositor);
                window->xdg = xdg_wm_base_get_xdg_surface(shell, window->surface);
                xdg_surface_add_listener(window->xdg, &surface_listener, window);
                window->top = xdg_surface_get_toplevel(window->xdg);
                xdg_toplevel_add_listener(window->top, &top_listener, window);
                char title[32]; snprintf(title, sizeof(title), "Layout-%d", id);
                xdg_toplevel_set_title(window->top, title);
                if (parent >= 0) { assert(parent < 8 && windows[parent].top); xdg_toplevel_set_parent(window->top, windows[parent].top); }
            }
            assert(window->top);
            xdg_toplevel_set_min_size(window->top, minw, minh);
            xdg_toplevel_set_max_size(window->top, maxw, maxh);
            wl_surface_commit(window->surface);
        }
    }
    for (int i = 7; i >= 0; i--) {
        if (windows[i].top) {
            xdg_toplevel_destroy(windows[i].top);
            xdg_surface_destroy(windows[i].xdg);
            wl_surface_destroy(windows[i].surface);
        }
    }
    wl_buffer_destroy(buffer); xdg_wm_base_destroy(shell);
    wl_shm_destroy(shm); wl_compositor_destroy(compositor); wl_registry_destroy(registry);
    wl_display_flush(display);
    wl_display_disconnect(display);
    return 0;
}
