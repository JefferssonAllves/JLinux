#ifndef JLINUX_RENDERER_H
#define JLINUX_RENDERER_H
#include <stdbool.h>
struct wl_display;
struct wlr_backend;
struct wlr_renderer;
struct wlr_allocator;
struct wlr_scene_output;

struct jlinux_renderer {
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
};

bool jlinux_renderer_init(struct jlinux_renderer *renderer,
    struct wlr_backend *backend, struct wl_display *display);
void jlinux_renderer_finish(struct jlinux_renderer *renderer);
bool jlinux_renderer_present(struct wlr_scene_output *output);
#endif
