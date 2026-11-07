#ifndef JLINUX_BROWSER_CHROME_H
#define JLINUX_BROWSER_CHROME_H
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wpe/wayland/wpe-wayland.h>
#include <wpe/webkit.h>
#include "../effects/glass.h"
#include "address.h"
#define BROWSER_CHROME_HEIGHT 100
#define BROWSER_TAB_HEIGHT 36
#define BROWSER_TAB_WIDTH 180
struct browser_chrome {
    WPEView *view;
    GPtrArray *tabs;
    guint active_tab;
    struct wl_registry *registry;
    struct wl_subcompositor *subcompositor;
    struct wl_surface *surface;
    struct wl_subsurface *subsurface;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
    gboolean pointer_inside;
    double pointer_x, pointer_y;
    void (*clicked)(struct browser_chrome *, double, double, gpointer);
    gpointer clicked_data;
    struct wl_list buffers;
    struct jlinux_glass glass;
    guint idle, caret_timer;
    gboolean caret_visible, cursor_text;
    uint32_t pointer_serial;
    int in_flight;
    gboolean editing, dirty, selected;
    gboolean adblock_available, adblock_enabled;
    char address[BROWSER_ADDRESS_LIMIT + 1];
    char message[256];
    char download_status[40];
};
gboolean browser_chrome_init(struct browser_chrome *chrome, WPEView *view);
void browser_chrome_update(struct browser_chrome *chrome);
void browser_chrome_finish(struct browser_chrome *chrome);
void browser_chrome_paint(struct browser_chrome *chrome, cairo_t *cr, int width);
double browser_chrome_tab_width(int width, guint count);
guint browser_chrome_tab_at(struct browser_chrome *chrome, int width, double x, gboolean *close);
gboolean browser_chrome_url_at(int width, double x, double y);
int browser_chrome_new_tab_x(struct browser_chrome *chrome, int width);
#endif
