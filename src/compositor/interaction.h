#ifndef JLINUX_INTERACTION_H
#define JLINUX_INTERACTION_H
struct wl_display;
struct wlr_backend;
struct wlr_session;
struct wlr_output_layout;
struct wlr_scene;
struct wlr_scene_tree;
struct interaction;
struct wlr_box;
struct interaction *interaction_create(struct wl_display *display,
    struct wlr_backend *backend, struct wlr_output_layout *layout,
    struct wlr_scene *scene, struct wlr_scene_tree *windows, struct wlr_session *session);
void interaction_set_workarea(struct interaction *interaction, const struct wlr_box *area);
void interaction_destroy(struct interaction *interaction);
#endif
