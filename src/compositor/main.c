#define _POSIX_C_SOURCE 200809L
#include "interaction.h"
#include "../toolkit/painter.h"
#include "../renderer/renderer.h"
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>

struct desktop {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_session *session;
    struct jlinux_renderer render;
    struct wlr_output_layout *layout;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    struct wl_listener new_output, new_layer, layout_change;
    struct wl_list monitors, layers;
    struct wlr_scene_tree *wallpapers, *layer_trees[4], *windows, *fullscreens;
    struct interaction *interaction;
    unsigned int output_count;
};

struct monitor {
    struct desktop *desktop;
    struct wlr_output *output;
    struct wlr_scene_output *scene_output;
    struct wl_listener frame, request_state, destroy;
    struct wl_list link;
    struct wlr_scene_rect *background;
};

struct shell_surface {
    struct desktop *desktop;
    struct wlr_layer_surface_v1 *layer;
    struct wlr_scene_layer_surface_v1 *scene;
    struct wl_list link;
    struct wl_listener commit, map, unmap, destroy;
};

static void arrange_desktop(struct desktop *desktop) {
    struct wlr_output *center = wlr_output_layout_get_center_output(desktop->layout);
    struct shell_surface *pending;
    wl_list_for_each(pending, &desktop->layers, link) {
        if (!pending->layer->output && center) {
            pending->layer->output = center;
            wlr_scene_node_set_enabled(&pending->scene->tree->node, pending->layer->surface->mapped);
        }
    }
    struct monitor *monitor;
    wl_list_for_each(monitor, &desktop->monitors, link) {
        struct wlr_box bounds;
        wlr_output_layout_get_box(desktop->layout, monitor->output, &bounds);
        if (bounds.width <= 0 || bounds.height <= 0) { continue; }
        wlr_scene_node_set_position(&monitor->background->node, bounds.x, bounds.y);
        wlr_scene_rect_set_size(monitor->background, bounds.width, bounds.height);
        struct wlr_box usable = bounds;
        for (int level = 0; level < 4; level++) {
            struct shell_surface *surface;
            wl_list_for_each(surface, &desktop->layers, link) {
                if (surface->layer->output == monitor->output &&
                    surface->layer->initialized && (int)surface->layer->current.layer == level) {
                    wlr_scene_layer_surface_v1_configure(surface->scene, &bounds, &usable);
                }
            }
        }
        if (monitor->output == wlr_output_layout_get_center_output(desktop->layout)) {
            interaction_set_workarea(desktop->interaction, &usable);
        }
    }
}

static void layout_changed(struct wl_listener *listener, void *data) {
    (void)data;
    struct desktop *desktop = wl_container_of(listener, desktop, layout_change);
    arrange_desktop(desktop);
}

static void shell_committed(struct wl_listener *listener, void *data) {
    (void)data;
    struct shell_surface *surface = wl_container_of(listener, surface, commit);
    uint32_t changed = surface->layer->current.committed;
    if (surface->layer->initial_commit || changed) {
        unsigned int level = surface->layer->current.layer;
        wlr_scene_node_reparent(&surface->scene->tree->node, surface->desktop->layer_trees[level]);
        arrange_desktop(surface->desktop);
    }
}

static void shell_mapped(struct wl_listener *listener, void *data) {
    (void)data;
    struct shell_surface *surface = wl_container_of(listener, surface, map);
    arrange_desktop(surface->desktop);
    wlr_log(WLR_INFO, "Interface mapeada: %s", surface->layer->namespace);
}

static void shell_unmapped(struct wl_listener *listener, void *data) {
    (void)data;
    struct shell_surface *surface = wl_container_of(listener, surface, unmap);
    arrange_desktop(surface->desktop);
}
static void shell_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct shell_surface *surface = wl_container_of(listener, surface, destroy);
    struct desktop *desktop = surface->desktop;
    wl_list_remove(&surface->commit.link);
    wl_list_remove(&surface->map.link);
    wl_list_remove(&surface->unmap.link);
    wl_list_remove(&surface->destroy.link);
    wl_list_remove(&surface->link);
    free(surface);
    arrange_desktop(desktop);
}

static void add_shell_surface(struct wl_listener *listener, void *data) {
    struct desktop *desktop = wl_container_of(listener, desktop, new_layer);
    struct wlr_layer_surface_v1 *layer = data;
    if (!layer->output) {
        layer->output = wlr_output_layout_get_center_output(desktop->layout);
    }
    struct shell_surface *surface = calloc(1, sizeof(*surface));
    if (!surface) {
        wlr_layer_surface_v1_destroy(layer);
        return;
    }
    surface->desktop = desktop;
    surface->layer = layer;
    surface->scene = wlr_scene_layer_surface_v1_create(desktop->layer_trees[layer->pending.layer], layer);
    if (!surface->scene) {
        free(surface);
        wlr_layer_surface_v1_destroy(layer);
        return;
    }
    wl_list_insert(desktop->layers.prev, &surface->link);
    surface->commit.notify = shell_committed;
    surface->map.notify = shell_mapped;
    surface->unmap.notify = shell_unmapped;
    surface->destroy.notify = shell_destroyed;
    wl_signal_add(&layer->surface->events.commit, &surface->commit);
    wl_signal_add(&layer->surface->events.map, &surface->map);
    wl_signal_add(&layer->surface->events.unmap, &surface->unmap);
    wl_signal_add(&layer->events.destroy, &surface->destroy);
}

static void render_frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct monitor *monitor = wl_container_of(listener, monitor, frame);
    if (!jlinux_renderer_present(monitor->scene_output)) {
        wlr_log(WLR_ERROR, "Falha ao apresentar quadro em %s", monitor->output->name);
        return;
    }
}

static void resize_output(struct wl_listener *listener, void *data) {
    struct monitor *monitor = wl_container_of(listener, monitor, request_state);
    const struct wlr_output_event_request_state *event = data;
    if (!wlr_output_commit_state(monitor->output, event->state)) {
        wlr_log(WLR_ERROR, "Estado de saída recusado: %s", monitor->output->name);
    }
}

static void remove_output(struct wl_listener *listener, void *data) {
    (void)data;
    struct monitor *monitor = wl_container_of(listener, monitor, destroy);
    struct desktop *desktop = monitor->desktop;
    wlr_log(WLR_INFO, "Saída removida: %s", monitor->output->name);
    wl_list_remove(&monitor->frame.link);
    wl_list_remove(&monitor->request_state.link);
    wl_list_remove(&monitor->destroy.link);
    wlr_scene_output_destroy(monitor->scene_output);
    struct shell_surface *surface, *temporary;
    wl_list_for_each_safe(surface, temporary, &monitor->desktop->layers, link) {
        if (surface->layer->output == monitor->output) {
            if (desktop->session) {
                surface->layer->output = NULL;
                wlr_scene_node_set_enabled(&surface->scene->tree->node, false);
            } else { wlr_layer_surface_v1_destroy(surface->layer); }
        }
    }
    wl_list_remove(&monitor->link);
    wlr_scene_node_destroy(&monitor->background->node);
    monitor->desktop->output_count--;
    if (monitor->desktop->output_count == 0 && !desktop->session) {
        wl_display_terminate(monitor->desktop->display);
    }
    free(monitor);
    arrange_desktop(desktop);
}

static void add_output(struct wl_listener *listener, void *data) {
    struct desktop *desktop = wl_container_of(listener, desktop, new_output);
    struct wlr_output *output = data;
    if (!wlr_output_init_render(output, desktop->render.allocator, desktop->render.renderer)) {
        wlr_log(WLR_ERROR, "Não foi possível inicializar saída %s", output->name);
        return;
    }
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
    if (mode) {
        wlr_output_state_set_mode(&state, mode);
    }
    bool committed = wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);
    if (!committed) {
        wlr_log(WLR_ERROR, "Não foi possível ativar saída %s", output->name);
        return;
    }
    struct monitor *monitor = calloc(1, sizeof(*monitor));
    if (!monitor) {
        wl_display_terminate(desktop->display);
        return;
    }
    monitor->desktop = desktop;
    const float color[] = {0.035f, 0.055f, 0.09f, 1.0f};
    monitor->background = wlr_scene_rect_create(desktop->wallpapers, 1, 1, color);
    if (!monitor->background) {
        free(monitor);
        wl_display_terminate(desktop->display);
        return;
    }
    monitor->output = output;
    struct wlr_output_layout_output *position = wlr_output_layout_add_auto(desktop->layout, output);
    monitor->scene_output = wlr_scene_output_create(desktop->scene, output);
    if (!position || !monitor->scene_output) {
        wlr_scene_node_destroy(&monitor->background->node);
        free(monitor);
        wl_display_terminate(desktop->display);
        return;
    }
    wlr_scene_output_layout_add_output(desktop->scene_layout, position, monitor->scene_output);
    monitor->frame.notify = render_frame;
    monitor->request_state.notify = resize_output;
    monitor->destroy.notify = remove_output;
    wl_signal_add(&output->events.frame, &monitor->frame);
    wl_signal_add(&output->events.request_state, &monitor->request_state);
    wl_signal_add(&output->events.destroy, &monitor->destroy);
    wl_list_insert(desktop->monitors.prev, &monitor->link);
    desktop->output_count++;
    arrange_desktop(desktop);
    wlr_output_schedule_frame(output);
    wlr_log(WLR_INFO, "Saída ativa: %s", output->name);
}

static int stop_server(int signal_number, void *data) {
    wlr_log(WLR_INFO, "Encerrando compositor por sinal %d", signal_number);
    wl_display_terminate(data);
    return 0;
}

int main(void) {
    wlr_log_init(WLR_INFO, NULL);
    struct desktop desktop = {0};
    wl_list_init(&desktop.new_output.link);
    wl_list_init(&desktop.new_layer.link);
    wl_list_init(&desktop.layout_change.link);
    wl_list_init(&desktop.monitors);
    wl_list_init(&desktop.layers);
    int result = EXIT_FAILURE;
    struct wl_event_source *interrupt = NULL, *terminate = NULL;
    desktop.display = wl_display_create();
    if (!desktop.display) {
        return result;
    }
    struct wl_event_loop *loop = wl_display_get_event_loop(desktop.display);
    desktop.backend = wlr_backend_autocreate(loop, &desktop.session);
    if (!desktop.backend) { goto cleanup; }
    if (!jlinux_renderer_init(&desktop.render, desktop.backend, desktop.display)) { goto cleanup; }
    desktop.layout = wlr_output_layout_create(desktop.display);
    desktop.scene = wlr_scene_create();
    if (!desktop.layout || !desktop.scene) { goto cleanup; }
    desktop.wallpapers = wlr_scene_tree_create(&desktop.scene->tree);
    if (!desktop.wallpapers) { goto cleanup; }
    for (int level = 0; level < 4; level++) {
        if (level == 2) {
            desktop.windows = wlr_scene_tree_create(&desktop.scene->tree);
            if (!desktop.windows) { goto cleanup; }
        }
        if (level == 3) {
            desktop.fullscreens = wlr_scene_tree_create(&desktop.scene->tree);
            if (!desktop.fullscreens) { goto cleanup; }
        }
        desktop.layer_trees[level] = wlr_scene_tree_create(&desktop.scene->tree);
        if (!desktop.layer_trees[level]) { goto cleanup; }
    }
    desktop.layout_change.notify = layout_changed;
    wl_signal_add(&desktop.layout->events.change, &desktop.layout_change);
    struct wlr_layer_shell_v1 *shell = wlr_layer_shell_v1_create(desktop.display, 4);
    desktop.interaction = interaction_create(desktop.display, desktop.backend, desktop.layout,
        desktop.scene, desktop.windows, desktop.fullscreens, desktop.session);
    if (!shell || !desktop.interaction) { goto cleanup; }
    desktop.new_layer.notify = add_shell_surface;
    wl_signal_add(&shell->events.new_surface, &desktop.new_layer);
    desktop.scene_layout = wlr_scene_attach_output_layout(desktop.scene, desktop.layout);
    if (!desktop.scene_layout ||
        !wlr_compositor_create(desktop.display, 5, desktop.render.renderer) ||
        !wlr_subcompositor_create(desktop.display)) { goto cleanup; }
    desktop.new_output.notify = add_output;
    wl_signal_add(&desktop.backend->events.new_output, &desktop.new_output);
    interrupt = wl_event_loop_add_signal(loop, SIGINT, stop_server, desktop.display);
    terminate = wl_event_loop_add_signal(loop, SIGTERM, stop_server, desktop.display);
    if (!interrupt || !terminate) { goto cleanup; }
    const char *socket = wl_display_add_socket_auto(desktop.display);
    if (!socket || !wlr_backend_start(desktop.backend)) { goto cleanup; }
    if (desktop.output_count == 0) {
        wlr_log(WLR_ERROR, "%s", "Nenhuma saída ativa; encerrando o protótipo");
        goto cleanup;
    }
    if (setenv("WAYLAND_DISPLAY", socket, 1) != 0) { goto cleanup; }
    const char *ready_value = getenv("JLINUX_READY_FD");
    if (ready_value) {
        char *end;
        errno = 0;
        long fd = strtol(ready_value, &end, 10);
        if (errno || *end || fd < 3 || fd > INT_MAX) { goto cleanup; }
        ssize_t written;
        do { written = write((int)fd, socket, strlen(socket)); } while (written < 0 && errno == EINTR);
        close((int)fd); unsetenv("JLINUX_READY_FD");
        if (written != (ssize_t)strlen(socket)) { goto cleanup; }
    }
    wlr_log(WLR_INFO, "JLinux compositor pronto: WAYLAND_DISPLAY=%s", socket);
    wl_display_run(desktop.display);
    result = EXIT_SUCCESS;
cleanup:
    if (interrupt) { wl_event_source_remove(interrupt); }
    if (terminate) { wl_event_source_remove(terminate); }
    wl_display_destroy_clients(desktop.display);
    interaction_destroy(desktop.interaction);
    desktop.interaction = NULL;
    wl_list_remove(&desktop.new_output.link);
    wl_list_remove(&desktop.new_layer.link);
    wl_list_remove(&desktop.layout_change.link);
    if (desktop.backend) { wlr_backend_destroy(desktop.backend); }
    if (desktop.scene) { wlr_scene_node_destroy(&desktop.scene->tree.node); }
    jlinux_renderer_finish(&desktop.render);
    wl_display_destroy(desktop.display);
    ui_finish();
    return result;
}
