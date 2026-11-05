#define _POSIX_C_SOURCE 200809L
#include "interaction.h"
#include <errno.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
extern char **environ;

struct view {
    struct interaction *owner;
    struct wlr_xdg_toplevel *xdg;
    struct wlr_scene_tree *tree;
    bool mapped;
    struct wl_list link;
    struct wl_listener map, unmap, commit, destroy, maximize, fullscreen;
};
struct keyboard {
    struct interaction *owner;
    struct wlr_keyboard *device;
    bool intercepted[KEY_CNT];
    struct wl_list link;
    struct wl_listener key, modifiers, destroy;
};
struct pointer {
    struct interaction *owner;
    struct wl_list link;
    struct wl_listener destroy;
};
struct popup {
    struct wlr_xdg_popup *xdg;
    struct wl_listener commit, destroy;
};
struct interaction {
    struct wl_display *display;
    struct wlr_scene *scene;
    struct wlr_scene_tree *windows;
    struct wlr_output_layout *layout;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *theme;
    struct wl_list views, keyboards, pointers;
    struct view *focused, *grabbed;
    unsigned int placement;
    uint32_t grab_button;
    bool resizing;
    double start_x, start_y;
    int window_x, window_y, width, height;
    struct wl_listener new_input, new_view, new_popup, virtual_keyboard, virtual_pointer;
    struct wl_listener motion, absolute, button, axis, frame, set_cursor, selection;
    struct wl_event_source *child_exit;
};

static void listen(struct wl_listener *listener, struct wl_signal *signal,
                   wl_notify_func_t callback) {
    listener->notify = callback;
    wl_signal_add(signal, listener);
}

static void focus(struct interaction *state, struct view *view) {
    if (view && !view->mapped) { return; }
    if (state->focused && state->focused != view) {
        wlr_xdg_toplevel_set_activated(state->focused->xdg, false);
    }
    state->focused = view;
    if (!view) {
        wlr_seat_keyboard_clear_focus(state->seat);
        return;
    }
    wlr_scene_node_raise_to_top(&view->tree->node);
    wl_list_remove(&view->link);
    wl_list_insert(&state->views, &view->link);
    wlr_xdg_toplevel_set_activated(view->xdg, true);
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(state->seat);
    struct wlr_keyboard_modifiers empty = {0};
    wlr_seat_keyboard_notify_enter(state->seat, view->xdg->base->surface,
        keyboard ? keyboard->keycodes : NULL, keyboard ? keyboard->num_keycodes : 0,
        keyboard ? &keyboard->modifiers : &empty);
}

static void focus_available(struct interaction *state) {
    struct view *view;
    wl_list_for_each(view, &state->views, link) {
        if (view->mapped) { focus(state, view); return; }
    }
    focus(state, NULL);
}

static void view_mapped(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, map);
    view->mapped = true;
    wlr_scene_node_set_enabled(&view->tree->node, true);
    focus(view->owner, view);
    wlr_log(WLR_INFO, "Janela mapeada: %s", view->xdg->title ? view->xdg->title : "sem título");
}

static void view_unmapped(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, unmap);
    view->mapped = false;
    wlr_scene_node_set_enabled(&view->tree->node, false);
    if (view->owner->grabbed == view) { view->owner->grabbed = NULL; }
    if (view->owner->focused == view) {
        focus_available(view->owner);
    }
}

static void view_committed(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, commit);
    if (view->xdg->base->initial_commit) {
        struct wlr_output *output = wlr_output_layout_get_center_output(view->owner->layout);
        struct wlr_box area;
        wlr_output_layout_get_box(view->owner->layout, output, &area);
        int width = area.width > 1100 ? 1000 : area.width - 80;
        int height = area.height > 760 ? 640 : area.height - 120;
        wlr_xdg_toplevel_set_size(view->xdg, width > 100 ? width : 100, height > 100 ? height : 100);
        unsigned int offset = (view->owner->placement++ % 5) * 28;
        wlr_scene_node_set_position(&view->tree->node, area.x + 40 + (int)offset, area.y + 80 + (int)offset);
    }
}

static void unsupported_state(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, maximize);
    if (view->xdg->base->initialized) { wlr_xdg_surface_schedule_configure(view->xdg->base); }
}
static void unsupported_fullscreen(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, fullscreen);
    if (view->xdg->base->initialized) { wlr_xdg_surface_schedule_configure(view->xdg->base); }
}

static void view_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, destroy);
    if (view->owner->grabbed == view) { view->owner->grabbed = NULL; }
    if (view->owner->focused == view) { focus(view->owner, NULL); }
    wl_list_remove(&view->link);
    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->commit.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->maximize.link);
    wl_list_remove(&view->fullscreen.link);
    wlr_scene_node_destroy(&view->tree->node);
    free(view);
}

static void add_view(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, new_view);
    struct view *view = calloc(1, sizeof(*view));
    if (!view) { wl_display_terminate(state->display); return; }
    view->owner = state;
    view->xdg = data;
    view->tree = wlr_scene_tree_create(state->windows);
    struct wlr_scene_tree *content = view->tree ? wlr_scene_xdg_surface_create(view->tree, view->xdg->base) : NULL;
    if (!content) {
        if (view->tree) { wlr_scene_node_destroy(&view->tree->node); }
        free(view);
        wl_display_terminate(state->display);
        return;
    }
    view->tree->node.data = view;
    view->xdg->base->data = content;
    wlr_scene_node_set_enabled(&view->tree->node, false);
    wl_list_insert(state->views.prev, &view->link);
    listen(&view->map, &view->xdg->base->surface->events.map, view_mapped);
    listen(&view->unmap, &view->xdg->base->surface->events.unmap, view_unmapped);
    listen(&view->commit, &view->xdg->base->surface->events.commit, view_committed);
    listen(&view->destroy, &view->xdg->events.destroy, view_destroyed);
    listen(&view->maximize, &view->xdg->events.request_maximize, unsupported_state);
    listen(&view->fullscreen, &view->xdg->events.request_fullscreen, unsupported_fullscreen);
}

static void popup_committed(struct wl_listener *listener, void *data) {
    (void)data;
    struct popup *popup = wl_container_of(listener, popup, commit);
    if (popup->xdg->base->initial_commit) { wlr_xdg_surface_schedule_configure(popup->xdg->base); }
}
static void popup_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct popup *popup = wl_container_of(listener, popup, destroy);
    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);
    free(popup);
}
static void add_popup(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wlr_xdg_popup *xdg = data;
    struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg->parent);
    if (!parent || !parent->data) { wlr_xdg_popup_destroy(xdg); return; }
    struct popup *popup = calloc(1, sizeof(*popup));
    if (!popup) { wlr_xdg_popup_destroy(xdg); return; }
    struct wlr_scene_tree *tree = wlr_scene_xdg_surface_create(parent->data, xdg->base);
    if (!tree) { free(popup); wlr_xdg_popup_destroy(xdg); return; }
    xdg->base->data = tree;
    popup->xdg = xdg;
    listen(&popup->commit, &xdg->base->surface->events.commit, popup_committed);
    listen(&popup->destroy, &xdg->events.destroy, popup_destroyed);
}

static void update_capabilities(struct interaction *state) {
    uint32_t capabilities = 0;
    if (!wl_list_empty(&state->keyboards)) { capabilities |= WL_SEAT_CAPABILITY_KEYBOARD; }
    if (!wl_list_empty(&state->pointers)) { capabilities |= WL_SEAT_CAPABILITY_POINTER; }
    wlr_seat_set_capabilities(state->seat, capabilities);
}

static void terminal(struct interaction *state) {
    (void)state;
    char *argv[] = {"foot", "--term=xterm-256color", "--title=JLinux Terminal", NULL};
    pid_t child;
    posix_spawnattr_t attributes;
    int error = posix_spawnattr_init(&attributes);
    if (error) { return; }
    sigset_t mask;
    sigemptyset(&mask);
    posix_spawnattr_setsigmask(&attributes, &mask);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK);
    error = posix_spawnp(&child, argv[0], NULL, &attributes, argv, environ);
    posix_spawnattr_destroy(&attributes);
    if (error) { wlr_log(WLR_ERROR, "Não foi possível abrir Foot: %s", strerror(error)); }
}

static bool shortcut(struct interaction *state, xkb_keysym_t symbol, uint32_t modifiers) {
    if ((modifiers & WLR_MODIFIER_LOGO) && symbol == XKB_KEY_Escape) {
        wl_display_terminate(state->display); return true;
    }
    if ((modifiers & WLR_MODIFIER_LOGO) && symbol == XKB_KEY_Return) {
        terminal(state); return true;
    }
    if ((modifiers & WLR_MODIFIER_LOGO) && (symbol == XKB_KEY_q || symbol == XKB_KEY_Q)) {
        if (state->focused) { wlr_xdg_toplevel_send_close(state->focused->xdg); }
        return true;
    }
    if ((modifiers & WLR_MODIFIER_ALT) && symbol == XKB_KEY_Tab) {
        struct view *view;
        wl_list_for_each_reverse(view, &state->views, link) {
            if (view->mapped) { focus(state, view); break; }
        }
        return true;
    }
    return false;
}

static void key_event(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;
    bool handled = false;
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        const xkb_keysym_t *symbols;
        int count = xkb_state_key_get_syms(keyboard->device->xkb_state, event->keycode + 8, &symbols);
        for (int index = 0; index < count; index++) {
            handled |= shortcut(keyboard->owner, symbols[index], wlr_keyboard_get_modifiers(keyboard->device));
        }
        if (event->keycode < KEY_CNT) { keyboard->intercepted[event->keycode] = handled; }
    } else if (event->keycode < KEY_CNT) {
        handled = keyboard->intercepted[event->keycode];
        keyboard->intercepted[event->keycode] = false;
    }
    if (!handled) {
        wlr_seat_set_keyboard(keyboard->owner->seat, keyboard->device);
        wlr_seat_keyboard_notify_key(keyboard->owner->seat, event->time_msec, event->keycode, event->state);
    }
}
static void modifiers_event(struct wl_listener *listener, void *data) {
    (void)data;
    struct keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->owner->seat, keyboard->device);
    wlr_seat_keyboard_notify_modifiers(keyboard->owner->seat, &keyboard->device->modifiers);
}
static void keyboard_removed(struct wl_listener *listener, void *data) {
    (void)data;
    struct keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
    struct interaction *state = keyboard->owner;
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
    if (!wl_list_empty(&state->keyboards)) {
        struct keyboard *next = wl_container_of(state->keyboards.next, next, link);
        wlr_seat_set_keyboard(state->seat, next->device);
    } else { wlr_seat_set_keyboard(state->seat, NULL); }
    update_capabilities(state);
}
static void add_keyboard(struct interaction *state, struct wlr_keyboard *device, bool virtual) {
    if (!virtual) {
        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = context ? xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
        bool valid = keymap && wlr_keyboard_set_keymap(device, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        if (!valid) { wlr_log(WLR_ERROR, "%s", "Keymap inválido"); return; }
    }
    struct keyboard *keyboard = calloc(1, sizeof(*keyboard));
    if (!keyboard) { wl_display_terminate(state->display); return; }
    keyboard->owner = state;
    keyboard->device = device;
    wlr_keyboard_set_repeat_info(device, 30, 400);
    listen(&keyboard->key, &device->events.key, key_event);
    listen(&keyboard->modifiers, &device->events.modifiers, modifiers_event);
    listen(&keyboard->destroy, &device->base.events.destroy, keyboard_removed);
    wl_list_insert(state->keyboards.prev, &keyboard->link);
    wlr_seat_set_keyboard(state->seat, device);
    update_capabilities(state);
    if (state->focused) { focus(state, state->focused); }
    wlr_log(WLR_INFO, "Teclado conectado: %s", device->base.name);
}
static void pointer_removed(struct wl_listener *listener, void *data) {
    (void)data;
    struct pointer *pointer = wl_container_of(listener, pointer, destroy);
    struct interaction *state = pointer->owner;
    wl_list_remove(&pointer->destroy.link);
    wl_list_remove(&pointer->link);
    free(pointer);
    update_capabilities(state);
}
static void add_pointer(struct interaction *state, struct wlr_input_device *device) {
    struct pointer *pointer = calloc(1, sizeof(*pointer));
    if (!pointer) { wl_display_terminate(state->display); return; }
    pointer->owner = state;
    listen(&pointer->destroy, &device->events.destroy, pointer_removed);
    wl_list_insert(state->pointers.prev, &pointer->link);
    wlr_cursor_attach_input_device(state->cursor, device);
    update_capabilities(state);
    wlr_log(WLR_INFO, "Ponteiro conectado: %s", device->name);
}
static void input_added(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, new_input);
    struct wlr_input_device *device = data;
    if (device->type == WLR_INPUT_DEVICE_KEYBOARD) { add_keyboard(state, wlr_keyboard_from_input_device(device), false); }
    if (device->type == WLR_INPUT_DEVICE_POINTER) { add_pointer(state, device); }
}
static void virtual_keyboard_added(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, virtual_keyboard);
    struct wlr_virtual_keyboard_v1 *keyboard = data;
    add_keyboard(state, &keyboard->keyboard, true);
}
static void virtual_pointer_added(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, virtual_pointer);
    struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
    add_pointer(state, &event->new_pointer->pointer.base);
}

static struct view *surface_at(struct interaction *state, struct wlr_surface **surface, double *x, double *y) {
    struct wlr_scene_node *node = wlr_scene_node_at(&state->scene->tree.node, state->cursor->x, state->cursor->y, x, y);
    *surface = NULL;
    if (!node || node->type != WLR_SCENE_NODE_BUFFER) { return NULL; }
    struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
    if (!scene_surface) { return NULL; }
    *surface = scene_surface->surface;
    while (node && !node->data) { node = node->parent ? &node->parent->node : NULL; }
    return node ? node->data : NULL;
}
static void pointer_moved(struct interaction *state, uint32_t time) {
    if (state->grabbed) {
        int dx = (int)(state->cursor->x - state->start_x);
        int dy = (int)(state->cursor->y - state->start_y);
        if (state->resizing) {
            int width = state->width + dx, height = state->height + dy;
            wlr_xdg_toplevel_set_size(state->grabbed->xdg, width > 100 ? width : 100, height > 80 ? height : 80);
        } else {
            wlr_scene_node_set_position(&state->grabbed->tree->node, state->window_x + dx, state->window_y + dy);
        }
        return;
    }
    double x, y;
    struct wlr_surface *surface;
    surface_at(state, &surface, &x, &y);
    if (surface) {
        wlr_seat_pointer_notify_enter(state->seat, surface, x, y);
        wlr_seat_pointer_notify_motion(state->seat, time, x, y);
    } else {
        wlr_seat_pointer_clear_focus(state->seat);
        wlr_cursor_set_xcursor(state->cursor, state->theme, "default");
    }
}
static void motion_event(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(state->cursor, &event->pointer->base, event->delta_x, event->delta_y);
    pointer_moved(state, event->time_msec);
}
static void absolute_event(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(state->cursor, &event->pointer->base, event->x, event->y);
    pointer_moved(state, event->time_msec);
}
static void button_event(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, button);
    struct wlr_pointer_button_event *event = data;
    if (state->grab_button) {
        if (event->state == WL_POINTER_BUTTON_STATE_RELEASED && event->button == state->grab_button) {
            state->grabbed = NULL;
            state->grab_button = 0;
            pointer_moved(state, event->time_msec);
        }
        return;
    }
    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        double x, y;
        struct wlr_surface *surface;
        struct view *view = surface_at(state, &surface, &x, &y);
        if (view) {
            focus(state, view);
            wlr_log(WLR_INFO, "Foco por clique: %s", view->xdg->title ? view->xdg->title : "sem título");
        }
        struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(state->seat);
        if (view && keyboard && (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_LOGO) &&
            (event->button == BTN_LEFT || event->button == BTN_RIGHT)) {
            state->grabbed = view;
            state->resizing = event->button == BTN_RIGHT;
            state->grab_button = event->button;
            state->start_x = state->cursor->x; state->start_y = state->cursor->y;
            state->window_x = view->tree->node.x; state->window_y = view->tree->node.y;
            state->width = view->xdg->base->geometry.width; state->height = view->xdg->base->geometry.height;
            return;
        }
    }
    wlr_seat_pointer_notify_button(state->seat, event->time_msec, event->button, event->state);
}
static void axis_event(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, axis);
    struct wlr_pointer_axis_event *event = data;
    wlr_seat_pointer_notify_axis(state->seat, event->time_msec, event->orientation,
        event->delta, event->delta_discrete, event->source, event->relative_direction);
}
static void frame_event(struct wl_listener *listener, void *data) {
    (void)data;
    struct interaction *state = wl_container_of(listener, state, frame);
    wlr_seat_pointer_notify_frame(state->seat);
}
static void cursor_requested(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, set_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    if (state->seat->pointer_state.focused_client == event->seat_client) {
        wlr_cursor_set_surface(state->cursor, event->surface, event->hotspot_x, event->hotspot_y);
    }
}
static void selection_requested(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(state->seat, event->source, event->serial);
}
static int children_exited(int number, void *data) {
    (void)number; (void)data;
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
    return 0;
}

static size_t listeners(struct interaction *state, struct wl_listener **items) {
    struct wl_listener *all[] = {&state->new_input, &state->new_view, &state->new_popup,
        &state->virtual_keyboard, &state->virtual_pointer, &state->motion, &state->absolute,
        &state->button, &state->axis, &state->frame, &state->set_cursor, &state->selection};
    size_t count = sizeof(all) / sizeof(all[0]);
    memcpy(items, all, sizeof(all));
    return count;
}
struct interaction *interaction_create(struct wl_display *display,
    struct wlr_backend *backend, struct wlr_output_layout *layout,
    struct wlr_scene *scene, struct wlr_scene_tree *windows) {
    struct interaction *state = calloc(1, sizeof(*state));
    if (!state) { return NULL; }
    state->display = display; state->scene = scene; state->windows = windows; state->layout = layout;
    wl_list_init(&state->views); wl_list_init(&state->keyboards); wl_list_init(&state->pointers);
    struct wl_listener *items[12];
    size_t count = listeners(state, items);
    for (size_t i = 0; i < count; i++) { wl_list_init(&items[i]->link); }
    state->seat = wlr_seat_create(display, "seat0");
    state->cursor = wlr_cursor_create();
    state->theme = wlr_xcursor_manager_create(NULL, 24);
    struct wlr_xdg_shell *shell = wlr_xdg_shell_create(display, 3);
    if (!state->seat || !state->cursor || !state->theme || !shell ||
        !wlr_xcursor_manager_load(state->theme, 1) || !wlr_data_device_manager_create(display)) {
        interaction_destroy(state); return NULL;
    }
    wlr_cursor_attach_output_layout(state->cursor, layout);
    wlr_cursor_set_xcursor(state->cursor, state->theme, "default");
    listen(&state->new_input, &backend->events.new_input, input_added);
    listen(&state->new_view, &shell->events.new_toplevel, add_view);
    listen(&state->new_popup, &shell->events.new_popup, add_popup);
    listen(&state->motion, &state->cursor->events.motion, motion_event);
    listen(&state->absolute, &state->cursor->events.motion_absolute, absolute_event);
    listen(&state->button, &state->cursor->events.button, button_event);
    listen(&state->axis, &state->cursor->events.axis, axis_event);
    listen(&state->frame, &state->cursor->events.frame, frame_event);
    listen(&state->set_cursor, &state->seat->events.request_set_cursor, cursor_requested);
    listen(&state->selection, &state->seat->events.request_set_selection, selection_requested);
    state->child_exit = wl_event_loop_add_signal(wl_display_get_event_loop(display), SIGCHLD, children_exited, state);
    if (!state->child_exit) { interaction_destroy(state); return NULL; }
    /* Virtual input is opt-in only, for isolated automated tests. */
    const char *test_input = getenv("JLINUX_TEST_INPUT");
    if (test_input && strcmp(test_input, "1") == 0) {
        struct wlr_virtual_keyboard_manager_v1 *keyboards = wlr_virtual_keyboard_manager_v1_create(display);
        struct wlr_virtual_pointer_manager_v1 *pointers = wlr_virtual_pointer_manager_v1_create(display);
        if (!keyboards || !pointers) { interaction_destroy(state); return NULL; }
        listen(&state->virtual_keyboard, &keyboards->events.new_virtual_keyboard, virtual_keyboard_added);
        listen(&state->virtual_pointer, &pointers->events.new_virtual_pointer, virtual_pointer_added);
    }
    return state;
}
void interaction_destroy(struct interaction *state) {
    if (!state) { return; }
    struct wl_listener *items[12];
    size_t count = listeners(state, items);
    for (size_t i = 0; i < count; i++) { wl_list_remove(&items[i]->link); }
    if (state->child_exit) { wl_event_source_remove(state->child_exit); }
    struct keyboard *keyboard, *next_keyboard;
    wl_list_for_each_safe(keyboard, next_keyboard, &state->keyboards, link) {
        keyboard_removed(&keyboard->destroy, NULL);
    }
    struct pointer *pointer, *next_pointer;
    wl_list_for_each_safe(pointer, next_pointer, &state->pointers, link) {
        pointer_removed(&pointer->destroy, NULL);
    }
    if (state->cursor) { wlr_cursor_destroy(state->cursor); }
    if (state->theme) { wlr_xcursor_manager_destroy(state->theme); }
    if (state->seat) { wlr_seat_destroy(state->seat); }
    free(state);
}
