#ifndef JLINUX_BROWSER_CHROME_H
#define JLINUX_BROWSER_CHROME_H
#include <wayland-client.h>
#include <wpe/wayland/wpe-wayland.h>
#include <wpe/webkit.h>
#include "../effects/glass.h"
#include "address.h"
#define BROWSER_CHROME_HEIGHT 148
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
    struct wl_list buffers;
    struct jlinux_glass glass;
    guint idle;
    int in_flight;
    gboolean visible, dirty, selected;
    char address[BROWSER_ADDRESS_LIMIT + 1];
    char message[256];
};
gboolean browser_chrome_init(struct browser_chrome *chrome, WPEView *view);
void browser_chrome_update(struct browser_chrome *chrome);
void browser_chrome_finish(struct browser_chrome *chrome);
void browser_chrome_paint(struct browser_chrome *chrome, cairo_t *cr, int width);
guint browser_chrome_tab_slots(int width);
#endif
