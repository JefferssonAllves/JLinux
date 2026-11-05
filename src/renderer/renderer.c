#define _POSIX_C_SOURCE 200809L
#include "renderer.h"
#include <stddef.h>
#include <time.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>

bool jlinux_renderer_init(struct jlinux_renderer *renderer,
    struct wlr_backend *backend, struct wl_display *display) {
    renderer->renderer = wlr_renderer_autocreate(backend);
    if (!renderer->renderer || !wlr_renderer_init_wl_display(renderer->renderer, display)) {
        jlinux_renderer_finish(renderer);
        return false;
    }
    renderer->allocator = wlr_allocator_autocreate(backend, renderer->renderer);
    if (!renderer->allocator) {
        jlinux_renderer_finish(renderer);
        return false;
    }
    return true;
}

void jlinux_renderer_finish(struct jlinux_renderer *renderer) {
    if (renderer->allocator) { wlr_allocator_destroy(renderer->allocator); }
    if (renderer->renderer) { wlr_renderer_destroy(renderer->renderer); }
    renderer->allocator = NULL;
    renderer->renderer = NULL;
}

bool jlinux_renderer_present(struct wlr_scene_output *output) {
    if (!wlr_scene_output_commit(output, NULL)) { return false; }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(output, &now);
    return true;
}
