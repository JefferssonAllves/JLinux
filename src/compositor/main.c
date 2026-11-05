#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>

struct desktop {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_output_layout *layout;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    struct wl_listener new_output;
    unsigned int output_count;
};

struct monitor {
    struct desktop *desktop;
    struct wlr_output *output;
    struct wlr_scene_output *scene_output;
    struct wl_listener frame, request_state, destroy;
};

static void render_frame(struct wl_listener *listener, void *data) {
    (void)data;
    struct monitor *monitor = wl_container_of(listener, monitor, frame);
    if (!wlr_scene_output_commit(monitor->scene_output, NULL)) {
        wlr_log(WLR_ERROR, "Falha ao apresentar quadro em %s", monitor->output->name);
        return;
    }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(monitor->scene_output, &now);
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
    wl_list_remove(&monitor->frame.link);
    wl_list_remove(&monitor->request_state.link);
    wl_list_remove(&monitor->destroy.link);
    monitor->desktop->output_count--;
    free(monitor);
}

static void add_output(struct wl_listener *listener, void *data) {
    struct desktop *desktop = wl_container_of(listener, desktop, new_output);
    struct wlr_output *output = data;
    if (!wlr_output_init_render(output, desktop->allocator, desktop->renderer)) {
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
    monitor->output = output;
    struct wlr_output_layout_output *position = wlr_output_layout_add_auto(desktop->layout, output);
    monitor->scene_output = wlr_scene_output_create(desktop->scene, output);
    if (!position || !monitor->scene_output) {
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
    desktop->output_count++;
    wlr_output_schedule_frame(output);
    wlr_log(WLR_INFO, "Saída ativa: %s", output->name);
}

static int stop_server(int signal_number, void *data) {
    (void)signal_number;
    wl_display_terminate(data);
    return 0;
}

int main(void) {
    wlr_log_init(WLR_INFO, NULL);
    struct desktop desktop = {0};
    wl_list_init(&desktop.new_output.link);
    int result = EXIT_FAILURE;
    struct wl_event_source *interrupt = NULL, *terminate = NULL;
    desktop.display = wl_display_create();
    if (!desktop.display) {
        return result;
    }
    struct wl_event_loop *loop = wl_display_get_event_loop(desktop.display);
    desktop.backend = wlr_backend_autocreate(loop, NULL);
    if (!desktop.backend) { goto cleanup; }
    desktop.renderer = wlr_renderer_autocreate(desktop.backend);
    if (!desktop.renderer || !wlr_renderer_init_wl_display(desktop.renderer, desktop.display)) {
        goto cleanup;
    }
    desktop.allocator = wlr_allocator_autocreate(desktop.backend, desktop.renderer);
    if (!desktop.allocator) { goto cleanup; }
    desktop.layout = wlr_output_layout_create(desktop.display);
    desktop.scene = wlr_scene_create();
    if (!desktop.layout || !desktop.scene) { goto cleanup; }
    desktop.scene_layout = wlr_scene_attach_output_layout(desktop.scene, desktop.layout);
    if (!desktop.scene_layout ||
        !wlr_compositor_create(desktop.display, 5, desktop.renderer) ||
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
    wlr_log(WLR_INFO, "JLinux compositor pronto: WAYLAND_DISPLAY=%s", socket);
    wl_display_run(desktop.display);
    result = EXIT_SUCCESS;
cleanup:
    if (interrupt) { wl_event_source_remove(interrupt); }
    if (terminate) { wl_event_source_remove(terminate); }
    wl_display_destroy_clients(desktop.display);
    wl_list_remove(&desktop.new_output.link);
    if (desktop.backend) { wlr_backend_destroy(desktop.backend); }
    if (desktop.scene) { wlr_scene_node_destroy(&desktop.scene->tree.node); }
    if (desktop.allocator) { wlr_allocator_destroy(desktop.allocator); }
    if (desktop.renderer) { wlr_renderer_destroy(desktop.renderer); }
    wl_display_destroy(desktop.display);
    return result;
}
