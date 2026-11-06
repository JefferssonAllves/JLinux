#define _GNU_SOURCE
#include "chrome.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <linux/input-event-codes.h>

struct chrome_buffer {
    struct browser_chrome *chrome;
    struct wl_buffer *wl;
    struct wl_list link;
    void *data;
    size_t size;
};
guint browser_chrome_tab_slots(int width) { return MAX(1, (width - 104) / BROWSER_TAB_WIDTH); }

int browser_chrome_new_tab_x(struct browser_chrome *chrome, int width) {
    guint slots = browser_chrome_tab_slots(width);
    guint count = chrome->tabs ? chrome->tabs->len : 1;
    guint first = chrome->active_tab / slots * slots;
    return 12 + MIN(count - first, slots) * BROWSER_TAB_WIDTH;
}

void browser_chrome_paint(struct browser_chrome *chrome, cairo_t *cr, int width) {
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_set_source_rgba(cr, 0, 0, 0, 0); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, .006, .009, .016, .88);
    cairo_paint(cr);
    guint slots = browser_chrome_tab_slots(width), first = chrome->active_tab / slots * slots;
    guint count = chrome->tabs ? chrome->tabs->len : 1;
    for (guint i = first; i < MIN(count, first + slots); i++) {
        double x = 12 + (i - first) * BROWSER_TAB_WIDTH;
        cairo_set_source_rgba(cr, .25, .34, .62, i == chrome->active_tab ? .42 : .12);
        cairo_rectangle(cr, x, 5, BROWSER_TAB_WIDTH - 5, 28); cairo_fill(cr);
        const char *title = chrome->tabs ? webkit_web_view_get_title(g_ptr_array_index(chrome->tabs, i)) : "Nova aba";
        if (!title || !*title) { title = "Nova aba"; }
        char *short_title = g_utf8_make_valid(title, MIN(strlen(title), 160));
        char *label = g_strdup_printf("%u  %s", i + 1, short_title); g_free(short_title);
        cairo_save(cr); cairo_rectangle(cr, x + 7, 7, BROWSER_TAB_WIDTH - 40, 24); cairo_clip(cr);
        ui_text(cr, label, x + 8, 11, 12, false, 0xe2eaff); cairo_restore(cr); g_free(label);
        ui_text(cr, "×", x + BROWSER_TAB_WIDTH - 26, 9, 16, false, 0xb5c4e2);
    }
    ui_text(cr, "‹", width - 91, 7, 22, false, 0xb5c4e2);
    ui_text(cr, "›", width - 63, 7, 22, false, 0xb5c4e2);
    ui_text(cr, "+", browser_chrome_new_tab_x(chrome, width) + 8, 8, 21, false, 0xe2eaff);
    cairo_save(cr); cairo_translate(cr, 0, BROWSER_TAB_HEIGHT);
    ui_text(cr, "‹", 16, 18, 25, false, 0xdce7ff);
    ui_text(cr, "›", 54, 18, 25, false, 0xdce7ff);
    ui_text(cr, "↻", 91, 21, 21, false, 0xdce7ff);
    cairo_set_source_rgba(cr, .015, .025, .06, .65);
    cairo_rectangle(cr, 133, 15, width - 250, 38); cairo_fill(cr);
    if (chrome->editing && chrome->selected) {
        cairo_set_source_rgba(cr, .28, .40, .78, .38);
        cairo_rectangle(cr, 140, 20, width - 266, 28); cairo_fill(cr);
    }
    cairo_save(cr); cairo_rectangle(cr, 141, 18, width - 264, 33); cairo_clip(cr);
    ui_text(cr, chrome->address[0] ? chrome->address : "Pesquise no Google ou digite um endereço", 145, 25, 14, false, 0xe2eaff);
    cairo_restore(cr);
    ui_text(cr, chrome->download_status[0] ? chrome->download_status : "↓", width - 105, 25, 12, false, 0xe2eaff);
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
    if (chrome->in_flight >= 2) { return G_SOURCE_REMOVE; }
    int width = wpe_view_get_width(chrome->view);
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    size_t size = (size_t)stride * BROWSER_CHROME_HEIGHT;
    int fd = memfd_create("jlinux-browser-chrome", MFD_CLOEXEC);
    if (fd < 0) { goto failed; }
    if (ftruncate(fd, (off_t)size)) { close(fd); goto failed; }
    void *mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { close(fd); goto failed; }
    WPEDisplayWayland *display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(chrome->view));
    struct wl_shm_pool *pool = wl_shm_create_pool(wpe_display_wayland_get_wl_shm(display), fd, (int)size);
    close(fd);
    if (!pool) { munmap(mapping, size); goto failed; }
    struct wl_buffer *wl = wl_shm_pool_create_buffer(pool, 0, width, BROWSER_CHROME_HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    if (!wl) { munmap(mapping, size); goto failed; }
    struct chrome_buffer *buffer = g_new0(struct chrome_buffer, 1);
    *buffer = (struct chrome_buffer){.chrome = chrome, .wl = wl, .data = mapping, .size = size};
    wl_list_insert(&chrome->buffers, &buffer->link); chrome->in_flight++;
    wl_buffer_add_listener(wl, &buffer_listener, buffer);
    cairo_surface_t *image = cairo_image_surface_create_for_data(mapping, CAIRO_FORMAT_ARGB32, width, BROWSER_CHROME_HEIGHT, stride);
    cairo_t *cr = cairo_create(image); browser_chrome_paint(chrome, cr, width);
    gboolean valid = cairo_status(cr) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr); cairo_surface_destroy(image);
    if (!valid) { free_buffer(buffer); goto failed; }
    wl_surface_attach(chrome->surface, wl, 0, 0);
    wl_surface_damage_buffer(chrome->surface, 0, 0, width, BROWSER_CHROME_HEIGHT); wl_surface_commit(chrome->surface);
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
static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
        struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    struct browser_chrome *chrome = data;
    chrome->pointer_inside = surface == chrome->surface;
    chrome->pointer_x = wl_fixed_to_double(x); chrome->pointer_y = wl_fixed_to_double(y);
    if (!chrome->pointer_inside || !chrome->cursor_theme) { return; }
    struct wl_cursor *cursor = wl_cursor_theme_get_cursor(chrome->cursor_theme, "left_ptr");
    if (!cursor || !cursor->image_count) { return; }
    struct wl_cursor_image *image = cursor->images[0];
    wl_pointer_set_cursor(pointer, serial, chrome->cursor_surface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach(chrome->cursor_surface, wl_cursor_image_get_buffer(image), 0, 0);
    wl_surface_damage(chrome->cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(chrome->cursor_surface);
}
static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    (void)pointer; (void)serial; (void)surface;
    ((struct browser_chrome *)data)->pointer_inside = FALSE;
}
static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)pointer; (void)time; struct browser_chrome *chrome = data;
    chrome->pointer_x = wl_fixed_to_double(x); chrome->pointer_y = wl_fixed_to_double(y);
}
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
        uint32_t time, uint32_t button, uint32_t state) {
    (void)pointer; (void)serial; (void)time; struct browser_chrome *chrome = data;
    if (chrome->pointer_inside && button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED && chrome->clicked) {
        chrome->clicked(chrome, chrome->pointer_x, chrome->pointer_y, chrome->clicked_data);
    }
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data; (void)pointer; (void)time; (void)axis; (void)value;
}
static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter, .leave = pointer_leave, .motion = pointer_motion,
    .button = pointer_button, .axis = pointer_axis,
};
static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
    struct browser_chrome *chrome = data;
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !chrome->pointer) {
        chrome->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(chrome->pointer, &pointer_listener, chrome);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && chrome->pointer) {
        wl_pointer_destroy(chrome->pointer); chrome->pointer = NULL; chrome->pointer_inside = FALSE;
    }
}
static const struct wl_seat_listener seat_listener = {.capabilities = seat_capabilities};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)version; struct browser_chrome *chrome = data;
    if (!strcmp(interface, wl_subcompositor_interface.name)) {
        chrome->subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    } else if (!strcmp(interface, wl_seat_interface.name) && !chrome->seat) {
        chrome->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(chrome->seat, &seat_listener, chrome);
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
    // Negative child coordinates extend the automatic xdg geometry above the unchanged page origin.
    wl_subsurface_set_position(chrome->subsurface, 0, -BROWSER_CHROME_HEIGHT);
    wl_subsurface_set_desync(chrome->subsurface);
    chrome->cursor_surface = wl_compositor_create_surface(compositor);
    if (!chrome->cursor_surface) { return FALSE; }
    chrome->cursor_theme = wl_cursor_theme_load(NULL, 24, wpe_display_wayland_get_wl_shm(display));
    draw(chrome);
    return TRUE;
}
void browser_chrome_finish(struct browser_chrome *chrome) {
    if (chrome->idle) { g_source_remove(chrome->idle); }
    if (chrome->pointer) { wl_pointer_destroy(chrome->pointer); }
    if (chrome->seat) { wl_seat_destroy(chrome->seat); }
    if (chrome->cursor_surface) { wl_surface_destroy(chrome->cursor_surface); }
    if (chrome->cursor_theme) { wl_cursor_theme_destroy(chrome->cursor_theme); }
    if (chrome->subsurface) { wl_subsurface_destroy(chrome->subsurface); }
    if (chrome->surface) { wl_surface_destroy(chrome->surface); }
    struct chrome_buffer *buffer, *tmp;
    wl_list_for_each_safe(buffer, tmp, &chrome->buffers, link) { free_buffer(buffer); }
    if (chrome->subcompositor) { wl_subcompositor_destroy(chrome->subcompositor); }
    if (chrome->registry) { wl_registry_destroy(chrome->registry); }
}
