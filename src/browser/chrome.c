#define _GNU_SOURCE
#include "chrome.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define CHROME_HEIGHT 112
struct chrome_buffer {
    struct browser_chrome *chrome;
    struct wl_buffer *wl;
    struct wl_list link;
    void *data;
    size_t size;
};

void browser_chrome_paint(struct browser_chrome *chrome, cairo_t *cr, int width) {
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_set_source_rgba(cr, 0, 0, 0, 0); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, .015, .025, .055, .96);
    cairo_rectangle(cr, 1, 1, width - 2, CHROME_HEIGHT - 2); cairo_fill(cr);
    cairo_push_group(cr);
    jlinux_glass_paint(cr, (struct ui_rect){1, 1, width - 2, CHROME_HEIGHT - 2}, 6, NULL, 0, 0, chrome->glass);
    cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, .5);
    ui_text(cr, "‹", 16, 18, 25, false, 0xdce7ff);
    ui_text(cr, "›", 54, 18, 25, false, 0xdce7ff);
    ui_text(cr, "↻", 91, 21, 21, false, 0xdce7ff);
    cairo_set_source_rgba(cr, .015, .025, .06, .65);
    cairo_rectangle(cr, 133, 15, width - 150, 38); cairo_fill(cr);
    if (chrome->selected) {
        cairo_set_source_rgba(cr, .28, .40, .78, .38);
        cairo_rectangle(cr, 140, 20, width - 166, 28); cairo_fill(cr);
    }
    cairo_save(cr); cairo_rectangle(cr, 141, 18, width - 164, 33); cairo_clip(cr);
    ui_text(cr, chrome->address[0] ? chrome->address : "Digite um endereço", 145, 25, 14, false, 0xe2eaff);
    cairo_restore(cr);
    cairo_save(cr); cairo_rectangle(cr, 15, 63, width - 30, 43); cairo_clip(cr);
    ui_text(cr, chrome->message[0] ? chrome->message : "Enter abrir  ·  Esc voltar à página", 17, 66, 12, false, 0xb5c4e2);
    ui_text(cr, "Ctrl+L endereço   ·   Alt+←/→ histórico   ·   Ctrl+R recarregar", 17, 87, 11, false, 0x92a5cb);
    cairo_restore(cr);
}
static void free_buffer(struct chrome_buffer *buffer) {
    wl_buffer_destroy(buffer->wl); munmap(buffer->data, buffer->size);
    wl_list_remove(&buffer->link); buffer->chrome->in_flight--; g_free(buffer);
}
static void released(void *data, struct wl_buffer *wl) {
    (void)wl; struct chrome_buffer *buffer = data; struct browser_chrome *chrome = buffer->chrome;
    free_buffer(buffer);
    if (chrome->dirty) { browser_chrome_update(chrome); }
}
static const struct wl_buffer_listener buffer_listener = {.release = released};
static gboolean draw(gpointer data) {
    struct browser_chrome *chrome = data; chrome->idle = 0;
    if (!chrome->surface) { return G_SOURCE_REMOVE; }
    if (!chrome->visible) {
        wl_surface_attach(chrome->surface, NULL, 0, 0); wl_surface_commit(chrome->surface);
        chrome->dirty = FALSE; return G_SOURCE_REMOVE;
    }
    if (chrome->in_flight >= 2) { return G_SOURCE_REMOVE; }
    int width = CLAMP(wpe_view_get_width(chrome->view) - 32, 320, 4096);
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    size_t size = (size_t)stride * CHROME_HEIGHT;
    int fd = memfd_create("jlinux-browser-chrome", MFD_CLOEXEC);
    if (fd < 0) { goto failed; }
    if (ftruncate(fd, (off_t)size)) { close(fd); goto failed; }
    void *mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { close(fd); goto failed; }
    WPEDisplayWayland *display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(chrome->view));
    struct wl_shm_pool *pool = wl_shm_create_pool(wpe_display_wayland_get_wl_shm(display), fd, (int)size);
    close(fd);
    if (!pool) { munmap(mapping, size); goto failed; }
    struct wl_buffer *wl = wl_shm_pool_create_buffer(pool, 0, width, CHROME_HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    if (!wl) { munmap(mapping, size); goto failed; }
    struct chrome_buffer *buffer = g_new0(struct chrome_buffer, 1);
    *buffer = (struct chrome_buffer){.chrome = chrome, .wl = wl, .data = mapping, .size = size};
    wl_list_insert(&chrome->buffers, &buffer->link); chrome->in_flight++;
    wl_buffer_add_listener(wl, &buffer_listener, buffer);
    cairo_surface_t *image = cairo_image_surface_create_for_data(mapping, CAIRO_FORMAT_ARGB32, width, CHROME_HEIGHT, stride);
    cairo_t *cr = cairo_create(image); browser_chrome_paint(chrome, cr, width);
    gboolean valid = cairo_status(cr) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr); cairo_surface_destroy(image);
    if (!valid) { free_buffer(buffer); goto failed; }
    wl_surface_attach(chrome->surface, wl, 0, 0);
    wl_surface_damage_buffer(chrome->surface, 0, 0, width, CHROME_HEIGHT); wl_surface_commit(chrome->surface);
    chrome->dirty = FALSE;
    return G_SOURCE_REMOVE;
failed:
    g_warning("Não foi possível desenhar a barra de navegação");
    return G_SOURCE_REMOVE;
}
void browser_chrome_update(struct browser_chrome *chrome) {
    chrome->dirty = TRUE;
    if (chrome->surface && !chrome->idle) { chrome->idle = g_idle_add(draw, chrome); }
}
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)version; struct browser_chrome *chrome = data;
    if (!strcmp(interface, wl_subcompositor_interface.name)) {
        chrome->subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    }
}
static void global_remove(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};
gboolean browser_chrome_init(struct browser_chrome *chrome, WPEView *view) {
    chrome->view = view; wl_list_init(&chrome->buffers);
    WPEDisplayWayland *display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    chrome->registry = wl_display_get_registry(wpe_display_wayland_get_wl_display(display));
    if (!chrome->registry) { return FALSE; }
    wl_registry_add_listener(chrome->registry, &registry_listener, chrome);
    if (wl_display_roundtrip(wpe_display_wayland_get_wl_display(display)) < 0 || !chrome->subcompositor) { return FALSE; }
    struct wl_compositor *compositor = wpe_display_wayland_get_wl_compositor(display);
    chrome->surface = wl_compositor_create_surface(compositor);
    if (!chrome->surface) { return FALSE; }
    chrome->subsurface = wl_subcompositor_get_subsurface(chrome->subcompositor, chrome->surface,
        wpe_view_wayland_get_wl_surface(WPE_VIEW_WAYLAND(view)));
    if (!chrome->subsurface) { return FALSE; }
    wl_subsurface_set_position(chrome->subsurface, 16, 16); wl_subsurface_set_desync(chrome->subsurface);
    // The parent WPE view routes input; its event handler consumes all overlay clicks.
    struct wl_region *empty = wl_compositor_create_region(compositor);
    if (!empty) { return FALSE; }
    wl_surface_set_input_region(chrome->surface, empty); wl_region_destroy(empty);
    return TRUE;
}
void browser_chrome_finish(struct browser_chrome *chrome) {
    if (chrome->idle) { g_source_remove(chrome->idle); }
    if (chrome->subsurface) { wl_subsurface_destroy(chrome->subsurface); }
    if (chrome->surface) { wl_surface_destroy(chrome->surface); }
    struct chrome_buffer *buffer, *tmp;
    wl_list_for_each_safe(buffer, tmp, &chrome->buffers, link) { free_buffer(buffer); }
    if (chrome->subcompositor) { wl_subcompositor_destroy(chrome->subcompositor); }
    if (chrome->registry) { wl_registry_destroy(chrome->registry); }
}
