#define _POSIX_C_SOURCE 200809L
#include "interaction.h"
#include "../renderer/window-frame.h"
#include "../renderer/window-effects.h"
#include "../animation/transition.h"
#include <time.h>
#include <wlr/types/wlr_output.h>
#include <errno.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include "../shell/control.h"
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
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
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/util/edges.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
extern char **environ;

struct view {
    struct interaction *owner;
    struct wlr_xdg_toplevel *xdg;
    struct wlr_scene_tree *tree, *visual, *content;
    struct window_aura aura;
    struct transition visibility, lift;
    bool mapped, maximized, is_fullscreen;
    unsigned workspace;
    bool snapped;
    struct wlr_box snap_restore;
    struct wlr_box restore, fullscreen_restore;
    int min_width, min_height, max_width, max_height;
    struct jlinux_window_frame frame;
    int frame_width, frame_height;
    struct wl_list link;
    struct wl_listener map, unmap, commit, destroy, maximize, fullscreen, title, parent;
};
struct decoration {
    struct wlr_xdg_toplevel_decoration_v1 *xdg;
    struct wl_listener mode, destroy, commit;
};
struct keyboard {
    struct interaction *owner;
    struct wlr_keyboard *device;
    bool intercepted[KEY_CNT];
    bool dock_requested;
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
struct closing_window {
    struct wl_list link;
    struct window_snapshot *snapshot;
    struct transition visibility;
    unsigned workspace;
};
struct interaction {
    struct wl_display *display;
    struct wlr_session *session;
    struct wlr_scene *scene;
    struct wlr_scene_tree *windows, *fullscreens;
    struct wlr_output_layout *layout;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *theme;
    struct wl_list views, keyboards, pointers, closing;
    struct window_hint hint;
    struct wlr_box hint_box;
    struct transition hint_visibility;
    bool reduced_motion;
    unsigned workspace;
    struct view *focused, *grabbed;
    struct wlr_box workarea;
    bool has_workarea;
    unsigned int placement;
    uint32_t grab_button;
    bool resizing;
    uint32_t resize_edges;
    struct jlinux_glass glass;
    double start_x, start_y;
    int window_x, window_y, width, height;
    struct wl_listener new_input, new_view, new_popup, virtual_keyboard, virtual_pointer, new_decoration;
    struct wl_listener motion, absolute, button, axis, frame, set_cursor, selection;
    struct wl_event_source *child_exit;
};

static double animation_time(void) {
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1e9;
}
static void schedule_animation(struct interaction *state) {
    struct wlr_output_layout_output *output;
    wl_list_for_each(output, &state->layout->outputs, link) { wlr_output_schedule_frame(output->output); }
}
static void animate(struct interaction *state, struct transition *transition, double target, double duration) {
    transition_target(transition, target, animation_time(), state->reduced_motion ? 0 : duration);
    schedule_animation(state);
}
static void buffer_opacity(struct wlr_scene_buffer *buffer, int x, int y, void *data) {
    (void)x; (void)y; wlr_scene_buffer_set_opacity(buffer, *(double *)data);
}
static void apply_visual(struct view *view, double now) {
    double visible = transition_value(&view->visibility, now);
    double lift = transition_value(&view->lift, now);
    wlr_scene_node_for_each_buffer(&view->content->node, buffer_opacity, &visible);
    for (unsigned i = 0; i < 4; i++) {
        wlr_scene_buffer_set_opacity(view->frame.parts[i], visible * (view->owner->focused == view ? 1 : .72));
    }
    wlr_scene_node_set_position(&view->visual->node, 0, view->owner->reduced_motion ? 0 : (int)((1-visible)*12-lift*3));
    window_aura_opacity(&view->aura, view->owner->focused == view, lift, visible);
    for (unsigned i = 0; i < 4; i++) {
        wlr_scene_node_set_enabled(&view->aura.shadow[i]->node, !view->is_fullscreen);
        wlr_scene_node_set_enabled(&view->aura.glow[i]->node, !view->is_fullscreen);
    }
}
bool interaction_animate(struct interaction *state) {
    if (!state) { return false; }
    double now = animation_time(); bool active = false;
    struct view *view;
    wl_list_for_each(view, &state->views, link) {
        if (!view->mapped || view->workspace != state->workspace) { continue; }
        apply_visual(view, now);
        active |= transition_active(&view->visibility, now) || transition_active(&view->lift, now);
    }
    struct closing_window *closing, *next;
    wl_list_for_each_safe(closing, next, &state->closing, link) {
        if (closing->workspace != state->workspace || !transition_active(&closing->visibility, now)) {
            window_snapshot_destroy(closing->snapshot); wl_list_remove(&closing->link); free(closing); continue;
        }
        double visible = transition_value(&closing->visibility, now);
        window_snapshot_apply(closing->snapshot, visible, (int)((1-visible)*8)); active = true;
    }
    window_hint_apply(&state->hint, state->hint_box, transition_value(&state->hint_visibility, now));
    return active || transition_active(&state->hint_visibility, now);
}
static void end_grab(struct interaction *state) {
    if (state->grabbed) { animate(state, &state->grabbed->lift, 0, .15); }
    state->grabbed = NULL; state->grab_button = 0;
    animate(state, &state->hint_visibility, 0, .12);
}
static void begin_grab(struct view *view) {
    view->visibility = (struct transition){.from=1, .to=1};
    apply_visual(view, animation_time());
    animate(view->owner, &view->lift, 1, .12);
}

static void listen(struct wl_listener *listener, struct wl_signal *signal,
                   wl_notify_func_t callback) {
    listener->notify = callback;
    wl_signal_add(signal, listener);
}

static void arrange(struct interaction *state);
static bool descendant(struct view *view, struct wlr_xdg_toplevel *parent);
static void fullscreen_requested(struct wl_listener *listener, void *data);

static void focus(struct interaction *state, struct view *view) {
    if (view && (!view->mapped || view->workspace != state->workspace)) { return; }
    if (state->focused && state->focused != view) {
        wlr_xdg_toplevel_set_activated(state->focused->xdg, false);
        jlinux_window_frame_focus(&state->focused->frame, false);
    }
    state->focused = view;
    schedule_animation(state);
    if (view) { jlinux_window_frame_focus(&view->frame, true); }
    if (!view) {
        wlr_seat_keyboard_clear_focus(state->seat);
        return;
    }
    wlr_scene_node_raise_to_top(&view->tree->node);
    struct view *child;
    wl_list_for_each_reverse(child, &state->views, link) {
        if (child->mapped && child->workspace == state->workspace && descendant(child, view->xdg)) {
            wlr_scene_node_raise_to_top(&child->tree->node);
        }
    }
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
        if (view->mapped && view->workspace == state->workspace) { focus(state, view); return; }
    }
    focus(state, NULL);
}

static void update_frame(struct view *view) {
    int width = view->xdg->base->geometry.width, height = view->xdg->base->geometry.height;
    if (width <= 0 || height <= 0) { return; }
    if (!jlinux_window_frame_update(&view->frame, width, height, view->xdg->title, view->owner->glass, view->maximized)) {
        wlr_log(WLR_ERROR, "%s", "Falha ao desenhar moldura da janela");
        wl_resource_post_no_memory(view->xdg->resource); return;
    }
    view->frame_width = width; view->frame_height = height;
    if (!window_aura_update(&view->aura, width, height, view->owner->glass)) {
        wlr_log(WLR_ERROR, "%s", "Falha ao desenhar sombra da janela");
    }
}
static bool maximize_area(struct view *view, struct wlr_box *area) {
    struct interaction *state = view->owner;
    if (state->has_workarea) { *area = state->workarea; }
    else {
        struct wlr_output *output = wlr_output_layout_get_center_output(state->layout);
        if (!output) { return false; }
        wlr_output_layout_get_box(state->layout, output, area);
    }
    area->x += JLINUX_FRAME_BORDER; area->y += JLINUX_FRAME_HEADER;
    area->width -= 2 * JLINUX_FRAME_BORDER;
    area->height -= JLINUX_FRAME_HEADER + JLINUX_FRAME_BORDER;
    return area->width > 0 && area->height > 0;
}
static void set_maximized(struct view *view, bool maximized) {
    if (!view->xdg->base->initialized || view->is_fullscreen) { return; }
    struct wlr_box area;
    if (maximized && !maximize_area(view, &area)) {
        wlr_xdg_surface_schedule_configure(view->xdg->base); return;
    }
    if (maximized && !view->maximized) {
        view->restore = (struct wlr_box){view->tree->node.x, view->tree->node.y,
            view->frame_width > 0 ? view->frame_width : view->xdg->scheduled.width,
            view->frame_height > 0 ? view->frame_height : view->xdg->scheduled.height};
    }
    if (!maximized && view->maximized) {
        area = view->restore;
        if (view->owner->has_workarea) {
            const struct wlr_box *work = &view->owner->workarea;
            if (area.x < work->x + JLINUX_FRAME_BORDER) { area.x = work->x + JLINUX_FRAME_BORDER; }
            if (area.y < work->y + JLINUX_FRAME_HEADER) { area.y = work->y + JLINUX_FRAME_HEADER; }
        }
    }
    if (maximized && !view->maximized) {
        wlr_log(WLR_INFO, "Janela maximizada: %s (%dx%d em %d,%d)",
            view->xdg->title ? view->xdg->title : "sem título", area.width, area.height, area.x, area.y);
    }
    if (maximized || view->maximized) {
        wlr_scene_node_set_position(&view->tree->node, area.x, area.y);
        wlr_xdg_toplevel_set_size(view->xdg, area.width, area.height);
    }
    if (view->owner->grabbed == view) { end_grab(view->owner); }
    view->maximized = maximized;
    if (maximized) { wlr_xdg_toplevel_set_tiled(view->xdg, 0); }
    wlr_xdg_toplevel_set_maximized(view->xdg, maximized);
    update_frame(view);
}
static void place(struct view *view, struct wlr_box box) {
    if (box.width < 1 || box.height < 1) { return; }
    wlr_scene_node_set_position(&view->tree->node, box.x, box.y);
    if (view->xdg->scheduled.width != box.width || view->xdg->scheduled.height != box.height) {
        wlr_xdg_toplevel_set_size(view->xdg, box.width, box.height);
    }
}
static void arrange(struct interaction *state) {
    struct view *view;
    wl_list_for_each(view, &state->views, link) {
        wlr_scene_node_set_enabled(&view->tree->node, view->mapped && view->workspace == state->workspace);
    }
}
static bool snap_target(struct interaction *state, struct wlr_box *target) {
    struct view *view = state->grabbed;
    if (!view || state->resizing || view->maximized || view->is_fullscreen) { return false; }
    struct wlr_output *output = wlr_output_layout_get_center_output(state->layout);
    if (!output) { return false; }
    struct wlr_box bounds, area;
    wlr_output_layout_get_box(state->layout, output, &bounds);
    if (state->cursor->y < bounds.y || state->cursor->y >= bounds.y + bounds.height || !maximize_area(view, &area)) { return false; }
    bool left = state->cursor->x <= bounds.x + 12;
    bool right = state->cursor->x >= bounds.x + bounds.width - 12;
    if (!left && !right) { return false; }
    int outer_width = area.width + 2 * JLINUX_FRAME_BORDER;
    int half = outer_width / 2;
    if (right) { area.x += half; }
    area.width = (right ? outer_width - half : half) - 2 * JLINUX_FRAME_BORDER;
    const struct wlr_xdg_toplevel_state *limits = &view->xdg->current;
    if (area.width < 1 || area.width < limits->min_width || area.height < limits->min_height) { return false; }
    if (limits->max_width > 0 && area.width > limits->max_width) { area.width = limits->max_width; }
    if (limits->max_height > 0 && area.height > limits->max_height) { area.height = limits->max_height; }
    *target = area; return true;
}
static void preview_snap(struct interaction *state) {
    struct wlr_box area;
    bool visible = snap_target(state, &area);
    if (visible) {
        state->hint_box = (struct wlr_box){area.x-JLINUX_FRAME_BORDER, area.y-JLINUX_FRAME_HEADER,
            area.width+2*JLINUX_FRAME_BORDER, area.height+JLINUX_FRAME_HEADER+JLINUX_FRAME_BORDER};
        wlr_scene_node_place_below(&state->hint.tree->node, &state->grabbed->tree->node);
    }
    double target = visible ? 1 : 0;
    if (state->hint_visibility.to != target) {
        animate(state, &state->hint_visibility, target, .16);
        if (visible) { wlr_log(WLR_INFO, "Prévia de encaixe: %dx%d em %d,%d", state->hint_box.width, state->hint_box.height, state->hint_box.x, state->hint_box.y); }
    }
}
static void snap_side(struct interaction *state) {
    struct view *view = state->grabbed;
    struct wlr_box area;
    if (!snap_target(state, &area)) {
        if (view && !state->resizing) { view->snap_restore = (struct wlr_box){0}; }
        return;
    }
    if (!view->snapped && !view->snap_restore.width) { view->snap_restore = (struct wlr_box){0, 0, state->width, state->height}; }
    view->snapped = true;
    wlr_xdg_toplevel_set_tiled(view->xdg, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
    place(view, area);
    wlr_log(WLR_INFO, "Janela encaixada: %s (%dx%d em %d,%d)", view->xdg->title ? view->xdg->title : "sem título", area.width, area.height, area.x, area.y);
}
static void detach_snap(struct view *view, bool resizing) {
    if (!view->snapped) { return; }
    view->snapped = false;
    wlr_xdg_toplevel_set_tiled(view->xdg, 0);
    if (resizing) { view->snap_restore = (struct wlr_box){0}; }
    else {
        struct wlr_box box = view->snap_restore;
        double ratio = (view->owner->cursor->x - view->tree->node.x) / (view->frame_width > 0 ? view->frame_width : 1);
        if (ratio < 0) { ratio = 0; } if (ratio > 1) { ratio = 1; }
        box.x = (int)(view->owner->cursor->x - box.width * ratio);
        box.y = (int)view->owner->cursor->y + JLINUX_FRAME_HEADER / 2;
        place(view, box);
    }
}
static bool descendant(struct view *view, struct wlr_xdg_toplevel *parent) {
    for (struct wlr_xdg_toplevel *p = view->xdg->parent; p; p = p->parent) {
        if (p == parent) { return true; }
    }
    return false;
}
static void parent_changed(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, parent), *parent;
    if (!view->xdg->parent) { return; }
    if (view->xdg->base->initialized) { wlr_xdg_toplevel_set_tiled(view->xdg, 0); }
    wl_list_for_each(parent, &view->owner->views, link) {
        if (parent->xdg == view->xdg->parent) {
            view->workspace = parent->workspace;
            wlr_scene_node_reparent(&view->tree->node, parent->tree->node.parent);
            wlr_scene_node_set_position(&view->tree->node,
                parent->tree->node.x + (parent->frame_width - view->frame_width) / 2,
                parent->tree->node.y + (parent->frame_height - view->frame_height) / 2);
            break;
        }
    }
    arrange(view->owner);
    if (view->mapped && view->workspace == view->owner->workspace) { focus(view->owner, view); }
    else if (view->owner->focused == view) { focus_available(view->owner); }
}
static void change_workspace(struct interaction *state, unsigned workspace, bool move) {
    if (move) {
        if (!state->focused) { return; }
        struct view *view;
        wl_list_for_each(view, &state->views, link) {
            if (view == state->focused || descendant(view, state->focused->xdg)) { view->workspace = workspace; }
        }
    } else { state->workspace = workspace; }
    end_grab(state);
    state->hint_visibility = (struct transition){0};
    wlr_scene_node_set_enabled(&state->hint.tree->node, false);
    arrange(state); focus_available(state);
    wlr_seat_pointer_clear_focus(state->seat);
}
static void title_changed(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, title);
    update_frame(view);
}
static void view_mapped(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, map);
    update_frame(view);
    view->mapped = true;
    parent_changed(&view->parent, NULL);
    wlr_scene_node_set_enabled(&view->tree->node, true);
    arrange(view->owner);
    focus(view->owner, view);
    view->visibility = (struct transition){0};
    animate(view->owner, &view->visibility, 1, .18);
    apply_visual(view, animation_time());
    wlr_log(WLR_INFO, "Janela mapeada: %s (%dx%d em %d,%d)",
        view->xdg->title ? view->xdg->title : "sem título", view->frame_width, view->frame_height,
        view->tree->node.x, view->tree->node.y);
}

static void view_unmapped(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, unmap);
    if (view->mapped && view->workspace == view->owner->workspace && !view->owner->reduced_motion) {
        struct closing_window *closing = calloc(1, sizeof(*closing));
        if (closing) {
            closing->snapshot = window_snapshot_create(view->tree, view->tree->node.parent);
            if (closing->snapshot) {
                closing->workspace = view->workspace;
                closing->visibility = (struct transition){.from=1, .to=1};
                wl_list_insert(&view->owner->closing, &closing->link);
                animate(view->owner, &closing->visibility, 0, .16);
                wlr_log(WLR_INFO, "Fechamento animado: %zu buffers", window_snapshot_count(closing->snapshot));
            } else { free(closing); }
        }
    }
    view->mapped = false;
    view->maximized = false;
    view->is_fullscreen = false;
    wlr_scene_node_reparent(&view->tree->node, view->owner->windows);
    for (unsigned i = 0; i < 4; i++) { wlr_scene_node_set_enabled(&view->frame.parts[i]->node, true); }
    wlr_scene_node_set_enabled(&view->tree->node, false);
    if (view->owner->grabbed == view) { end_grab(view->owner); }
    if (view->owner->focused == view) {
        focus_available(view->owner);
    }
    arrange(view->owner);
}

static void view_committed(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, commit);
    if (view->xdg->base->geometry.width != view->frame_width ||
        view->xdg->base->geometry.height != view->frame_height) { update_frame(view); }
    const struct wlr_xdg_toplevel_state *limits = &view->xdg->current;
    bool changed = view->min_width != limits->min_width || view->min_height != limits->min_height ||
        view->max_width != limits->max_width || view->max_height != limits->max_height;
    view->min_width = limits->min_width; view->min_height = limits->min_height;
    view->max_width = limits->max_width; view->max_height = limits->max_height;
    if (changed && view->mapped && !view->maximized && !view->is_fullscreen) {
        int width = view->frame_width, height = view->frame_height;
        if (width < limits->min_width) { width = limits->min_width; }
        if (height < limits->min_height) { height = limits->min_height; }
        if (limits->max_width > 0 && width > limits->max_width) { width = limits->max_width; }
        if (limits->max_height > 0 && height > limits->max_height) { height = limits->max_height; }
        place(view, (struct wlr_box){view->tree->node.x, view->tree->node.y, width, height});
    }
    if (view->xdg->base->initial_commit) {
        struct wlr_output *output = wlr_output_layout_get_center_output(view->owner->layout);
        struct wlr_box area;
        wlr_output_layout_get_box(view->owner->layout, output, &area);
        if (view->owner->has_workarea) { area = view->owner->workarea; }
        int width = area.width > 1100 ? 1000 : area.width - 80;
        int height = area.height > 760 ? 640 : area.height - 120;
        if (width < limits->min_width) { width = limits->min_width; }
        if (height < limits->min_height) { height = limits->min_height; }
        if (limits->max_width > 0 && width > limits->max_width) { width = limits->max_width; }
        if (limits->max_height > 0 && height > limits->max_height) { height = limits->max_height; }
        wlr_xdg_toplevel_set_size(view->xdg, width > 0 ? width : 1, height > 0 ? height : 1);
        unsigned int offset = (view->owner->placement++ % 5) * 28;
        wlr_scene_node_set_position(&view->tree->node, area.x + 40 + (int)offset, area.y + 80 + (int)offset);
        if (view->xdg->requested.maximized) { set_maximized(view, true); }
        if (view->xdg->requested.fullscreen) { fullscreen_requested(&view->fullscreen, NULL); }
    }
}

static void maximize_requested(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, maximize);
    set_maximized(view, view->xdg->requested.maximized);
    arrange(view->owner);
}
static void fullscreen_requested(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, fullscreen);
    if (!view->xdg->base->initialized) { return; }
    bool enabled = view->xdg->requested.fullscreen;
    bool changed = view->is_fullscreen != enabled;
    if (enabled && !view->is_fullscreen) {
        view->fullscreen_restore = (struct wlr_box){view->tree->node.x, view->tree->node.y,
            view->frame_width > 0 ? view->frame_width : view->xdg->scheduled.width,
            view->frame_height > 0 ? view->frame_height : view->xdg->scheduled.height};
    }
    if (view->owner->grabbed == view) { end_grab(view->owner); }
    view->is_fullscreen = enabled;
    schedule_animation(view->owner);
    wlr_xdg_toplevel_set_fullscreen(view->xdg, enabled);
    wlr_scene_node_reparent(&view->tree->node, enabled ? view->owner->fullscreens : view->owner->windows);
    for (unsigned i = 0; i < 4; i++) { wlr_scene_node_set_enabled(&view->frame.parts[i]->node, !enabled); }
    if (enabled) {
        wlr_xdg_toplevel_set_tiled(view->xdg, 0);
        struct wlr_box box;
        struct wlr_output *output = wlr_output_layout_get_center_output(view->owner->layout);
        if (output) { wlr_output_layout_get_box(view->owner->layout, output, &box); place(view, box); }
    } else {
        place(view, view->fullscreen_restore);
        if (view->maximized) { set_maximized(view, true); }
    }
    if (changed) {
        struct view *child;
        wl_list_for_each(child, &view->owner->views, link) {
            if (!child->is_fullscreen && descendant(child, view->xdg)) {
                wlr_scene_node_reparent(&child->tree->node, view->tree->node.parent);
            }
        }
        arrange(view->owner);
    }
}

static void view_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct view *view = wl_container_of(listener, view, destroy);
    if (view->owner->grabbed == view) { end_grab(view->owner); }
    if (view->owner->focused == view) { focus(view->owner, NULL); }
    wl_list_remove(&view->link);
    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->commit.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->maximize.link);
    wl_list_remove(&view->fullscreen.link);
    wl_list_remove(&view->title.link);
    wl_list_remove(&view->parent.link);
    wlr_scene_node_destroy(&view->tree->node);
    free(view);
}

static void add_view(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, new_view);
    struct view *view = calloc(1, sizeof(*view));
    if (!view) { wl_display_terminate(state->display); return; }
    view->owner = state;
    view->xdg = data;
    view->workspace = state->workspace;
    view->tree = wlr_scene_tree_create(state->windows);
    view->visual = view->tree ? wlr_scene_tree_create(view->tree) : NULL;
    bool framed = view->visual && window_aura_create(&view->aura, view->visual) && jlinux_window_frame_create(&view->frame, view->visual);
    struct wlr_scene_tree *content = framed ? wlr_scene_xdg_surface_create(view->visual, view->xdg->base) : NULL;
    view->content = content;
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
    jlinux_window_frame_focus(&view->frame, false);
    listen(&view->parent, &view->xdg->events.set_parent, parent_changed);
    listen(&view->title, &view->xdg->events.set_title, title_changed);
    listen(&view->map, &view->xdg->base->surface->events.map, view_mapped);
    listen(&view->unmap, &view->xdg->base->surface->events.unmap, view_unmapped);
    /* Capture buffers before wlroots disables its xdg subtree on unmap. */
    wl_list_remove(&view->unmap.link);
    wl_list_insert(&view->xdg->base->surface->events.unmap.listener_list, &view->unmap.link);
    listen(&view->commit, &view->xdg->base->surface->events.commit, view_committed);
    listen(&view->destroy, &view->xdg->events.destroy, view_destroyed);
    listen(&view->maximize, &view->xdg->events.request_maximize, maximize_requested);
    listen(&view->fullscreen, &view->xdg->events.request_fullscreen, fullscreen_requested);
}

static void decoration_mode(struct wl_listener *listener, void *data) {
    (void)data;
    struct decoration *decoration = wl_container_of(listener, decoration, mode);
    if (!decoration->xdg->toplevel->base->initialized) { return; }
    wlr_xdg_toplevel_decoration_v1_set_mode(decoration->xdg, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}
static void decoration_committed(struct wl_listener *listener, void *data) {
    (void)data;
    struct decoration *decoration = wl_container_of(listener, decoration, commit);
    if (decoration->xdg->toplevel->base->initial_commit) { decoration_mode(&decoration->mode, NULL); }
}
static void decoration_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct decoration *decoration = wl_container_of(listener, decoration, destroy);
    wl_list_remove(&decoration->mode.link); wl_list_remove(&decoration->destroy.link);
    wl_list_remove(&decoration->commit.link); free(decoration);
}
static void decoration_added(struct wl_listener *listener, void *data) {
    (void)listener;
    struct decoration *decoration = calloc(1, sizeof(*decoration));
    if (!decoration) { return; }
    decoration->xdg = data;
    listen(&decoration->commit, &decoration->xdg->toplevel->base->surface->events.commit, decoration_committed);
    listen(&decoration->mode, &decoration->xdg->events.request_mode, decoration_mode);
    listen(&decoration->destroy, &decoration->xdg->events.destroy, decoration_destroyed);
    decoration_mode(&decoration->mode, NULL);
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
    if (state->session && symbol >= XKB_KEY_XF86Switch_VT_1 && symbol <= XKB_KEY_XF86Switch_VT_12) {
        wlr_session_change_vt(state->session, symbol - XKB_KEY_XF86Switch_VT_1 + 1); return true;
    }
    if ((modifiers & WLR_MODIFIER_LOGO) && symbol >= XKB_KEY_1 && symbol <= XKB_KEY_9) {
        change_workspace(state, symbol - XKB_KEY_1, modifiers & WLR_MODIFIER_SHIFT); return true;
    }
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
    if ((modifiers & WLR_MODIFIER_LOGO) && (symbol == XKB_KEY_Up || symbol == XKB_KEY_Down)) {
        if (state->focused) { set_maximized(state->focused, symbol == XKB_KEY_Up); arrange(state); }
        return true;
    }
    if ((modifiers & WLR_MODIFIER_ALT) && symbol == XKB_KEY_Tab) {
        struct view *view;
        wl_list_for_each_reverse(view, &state->views, link) {
            if (view->mapped && view->workspace == state->workspace) { focus(state, view); break; }
        }
        return true;
    }
    return false;
}

static void update_dock(struct interaction *state) {
    struct keyboard *keys;bool visible=false;
    wl_list_for_each(keys,&state->keyboards,link){visible|=keys->dock_requested;}
    if(!shell_control_dock(visible)){wlr_log(WLR_DEBUG,"%s","Shell sem controle de dock disponível");}
}

static void cancel_super(struct interaction *state) {
    struct keyboard *keys;
    bool was_visible=false;
    wl_list_for_each(keys, &state->keyboards, link) { was_visible|=keys->dock_requested;keys->dock_requested = false; }
    if(was_visible){update_dock(state);}
}

static void key_event(struct wl_listener *listener, void *data) {
    struct keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;
    bool handled = false;
    const xkb_keysym_t *symbols;
    int count = xkb_state_key_get_syms(keyboard->device->xkb_state, event->keycode + 8, &symbols);
    bool super = false;
    for (int i = 0; i < count; i++) {
        super |= symbols[i] == XKB_KEY_Super_L || symbols[i] == XKB_KEY_Super_R;
    }
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (super) {
            uint32_t mods = wlr_keyboard_get_modifiers(keyboard->device);
            keyboard->dock_requested = !(mods & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT));
            update_dock(keyboard->owner);
        } else { cancel_super(keyboard->owner); }
    } else if (super) {
        keyboard->dock_requested = false;
        update_dock(keyboard->owner);
    }
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int index = 0; index < count; index++) {
            uint32_t mods = wlr_keyboard_get_modifiers(keyboard->device);
            xkb_keysym_t symbol = symbols[index];
            if ((mods & WLR_MODIFIER_LOGO) && event->keycode >= KEY_1 && event->keycode <= KEY_9) {
                symbol = XKB_KEY_1 + event->keycode - KEY_1;
            }
            handled |= shortcut(keyboard->owner, symbol, mods);
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
    if(keyboard->dock_requested){keyboard->dock_requested=false;update_dock(state);}
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
    if (state->grabbed && wl_list_empty(&state->pointers)) { end_grab(state); }
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
    if (scene_surface) { *surface = scene_surface->surface; }
    while (node && !node->data) { node = node->parent ? &node->parent->node : NULL; }
    return node ? node->data : NULL;
}
static void pointer_moved(struct interaction *state, uint32_t time) {
    if (state->grabbed) {
        int dx = (int)(state->cursor->x - state->start_x);
        int dy = (int)(state->cursor->y - state->start_y);
        if (state->resizing) {
            int width = state->width, height = state->height;
            if (state->resize_edges & WLR_EDGE_RIGHT) { width += dx; }
            if (state->resize_edges & WLR_EDGE_LEFT) { width -= dx; }
            if (state->resize_edges & WLR_EDGE_BOTTOM) { height += dy; }
            if (width < 100) { width = 100; } if (height < 80) { height = 80; }
            if (state->resize_edges & WLR_EDGE_LEFT) {
                wlr_scene_node_set_position(&state->grabbed->tree->node,
                    state->window_x + state->width - width, state->window_y);
            }
            wlr_xdg_toplevel_set_size(state->grabbed->xdg, width, height);
        } else {
            wlr_scene_node_set_position(&state->grabbed->tree->node, state->window_x + dx, state->window_y + dy);
        }
        preview_snap(state);
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
            snap_side(state);
            end_grab(state);
            pointer_moved(state, event->time_msec);
        }
        return;
    }
    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        double x, y;
        struct wlr_surface *surface;
        struct view *view = surface_at(state, &surface, &x, &y);
        if (view) {
            cancel_super(state);
            focus(state, view);
            wlr_log(WLR_INFO, "Foco por clique: %s", view->xdg->title ? view->xdg->title : "sem título");
        }
        if (view && !surface && event->button == BTN_LEFT) {
            double local_x = state->cursor->x - view->tree->node.x;
            double local_y = state->cursor->y - view->tree->node.y - view->visual->node.y;
            state->grab_button = event->button;
            if (local_y < 0 && local_x >= view->frame_width - 40 && local_x < view->frame_width) {
                wlr_xdg_toplevel_send_close(view->xdg); return;
            }
            if (local_y < 0 && local_x >= view->frame_width - 76 && local_x < view->frame_width - 40) {
                set_maximized(view, !view->maximized); arrange(state); return;
            }
            if (view->maximized || view->is_fullscreen) { return; }
            state->resize_edges = 0;
            if (local_x < 0) { state->resize_edges |= WLR_EDGE_LEFT; }
            if (local_x >= view->frame_width) { state->resize_edges |= WLR_EDGE_RIGHT; }
            if (local_y >= view->frame_height) { state->resize_edges |= WLR_EDGE_BOTTOM; }
            detach_snap(view, state->resize_edges != 0);
            state->grabbed = view; state->resizing = state->resize_edges != 0;
            state->start_x = state->cursor->x; state->start_y = state->cursor->y;
            state->window_x = view->tree->node.x; state->window_y = view->tree->node.y;
            state->width = view->frame_width; state->height = view->frame_height;
            begin_grab(view);
            return;
        }
        struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(state->seat);
        if (view && !view->maximized && !view->is_fullscreen && keyboard && (wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_LOGO) &&
            (event->button == BTN_LEFT || event->button == BTN_RIGHT)) {
            detach_snap(view, event->button == BTN_RIGHT);
            state->grabbed = view;
            state->resizing = event->button == BTN_RIGHT;
            state->resize_edges = WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM;
            state->grab_button = event->button;
            state->start_x = state->cursor->x; state->start_y = state->cursor->y;
            state->window_x = view->tree->node.x; state->window_y = view->tree->node.y;
            state->width = view->xdg->base->geometry.width; state->height = view->xdg->base->geometry.height;
            begin_grab(view);
            return;
        }
    }
    wlr_seat_pointer_notify_button(state->seat, event->time_msec, event->button, event->state);
}
static void axis_event(struct wl_listener *listener, void *data) {
    struct interaction *state = wl_container_of(listener, state, axis);
    cancel_super(state);
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
        &state->virtual_keyboard, &state->virtual_pointer, &state->new_decoration, &state->motion, &state->absolute,
        &state->button, &state->axis, &state->frame, &state->set_cursor, &state->selection};
    size_t count = sizeof(all) / sizeof(all[0]);
    memcpy(items, all, sizeof(all));
    return count;
}
struct interaction *interaction_create(struct wl_display *display,
    struct wlr_backend *backend, struct wlr_output_layout *layout,
    struct wlr_scene *scene, struct wlr_scene_tree *windows, struct wlr_scene_tree *fullscreens, struct wlr_session *session) {
    struct interaction *state = calloc(1, sizeof(*state));
    if (!state) { return NULL; }
    if (!jlinux_glass_settings(&state->glass)) { free(state); return NULL; }
    const char *reduced = getenv("JLINUX_REDUCED_MOTION");
    if (reduced && strcmp(reduced, "0") && strcmp(reduced, "1")) { free(state); return NULL; }
    state->reduced_motion = reduced && !strcmp(reduced, "1");
    state->session = session;
    state->display = display; state->scene = scene; state->windows = windows; state->fullscreens = fullscreens; state->layout = layout;
    wl_list_init(&state->views); wl_list_init(&state->keyboards); wl_list_init(&state->pointers); wl_list_init(&state->closing);
    struct wl_listener *items[13];
    size_t count = listeners(state, items);
    for (size_t i = 0; i < count; i++) { wl_list_init(&items[i]->link); }
    if (!window_hint_create(&state->hint, windows)) { interaction_destroy(state); return NULL; }
    state->seat = wlr_seat_create(display, "seat0");
    state->cursor = wlr_cursor_create();
    state->theme = wlr_xcursor_manager_create(NULL, 24);
    struct wlr_xdg_decoration_manager_v1 *decorations = wlr_xdg_decoration_manager_v1_create(display);
    struct wlr_xdg_shell *shell = wlr_xdg_shell_create(display, 3);
    if (!state->seat || !state->cursor || !state->theme || !shell || !decorations ||
        !wlr_xcursor_manager_load(state->theme, 1) || !wlr_data_device_manager_create(display)) {
        interaction_destroy(state); return NULL;
    }
    wlr_cursor_attach_output_layout(state->cursor, layout);
    wlr_cursor_set_xcursor(state->cursor, state->theme, "default");
    listen(&state->new_input, &backend->events.new_input, input_added);
    listen(&state->new_decoration, &decorations->events.new_toplevel_decoration, decoration_added);
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
void interaction_set_workarea(struct interaction *state, const struct wlr_box *area) {
    if (!state || area->width <= 0 || area->height <= 0) { return; }
    if (state->has_workarea && memcmp(&state->workarea, area, sizeof(*area)) == 0) { return; }
    end_grab(state);
    state->hint_visibility = (struct transition){0};
    wlr_scene_node_set_enabled(&state->hint.tree->node, false);
    state->workarea = *area; state->has_workarea = true;
    struct view *view;
    wl_list_for_each(view, &state->views, link) {
        if (view->is_fullscreen) { fullscreen_requested(&view->fullscreen, NULL); continue; }
        if (view->maximized) { set_maximized(view, true); continue; }
        int x = view->tree->node.x, y = view->tree->node.y;
        if (x - JLINUX_FRAME_BORDER < area->x) { x = area->x + JLINUX_FRAME_BORDER + 8; }
        if (y - JLINUX_FRAME_HEADER < area->y) { y = area->y + JLINUX_FRAME_HEADER + 8; }
        wlr_scene_node_set_position(&view->tree->node, x, y);
    }
    arrange(state);
}
void interaction_destroy(struct interaction *state) {
    if (!state) { return; }
    struct wl_listener *items[13];
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
    struct closing_window *closing, *next;
    wl_list_for_each_safe(closing, next, &state->closing, link) {
        window_snapshot_destroy(closing->snapshot); wl_list_remove(&closing->link); free(closing);
    }
    if (state->hint.tree) { wlr_scene_node_destroy(&state->hint.tree->node); }
    if (state->cursor) { wlr_cursor_destroy(state->cursor); }
    if (state->theme) { wlr_xcursor_manager_destroy(state->theme); }
    if (state->seat) { wlr_seat_destroy(state->seat); }
    free(state);
}
