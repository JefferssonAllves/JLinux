#ifndef JLINUX_WINDOW_VISUALS_H
#define JLINUX_WINDOW_VISUALS_H
#include "../effects/glass.h"
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
struct window_aura {
    struct wlr_scene_buffer *shadow[4], *glow[4];
    int width,height;
    bool shadow_enabled,glow_enabled,tint;
};
bool window_aura_create(struct window_aura *aura,struct wlr_scene_tree *parent);
bool window_aura_update(struct window_aura *aura,int width,int height,struct jlinux_glass effects);
void window_aura_opacity(struct window_aura *aura,bool focused,double lift,double visible);
struct wlr_scene_buffer *window_image_create(struct wlr_scene_tree *parent,cairo_surface_t *image);
struct window_snapshot;
struct window_snapshot *window_snapshot_create(struct wlr_scene_tree *source,struct wlr_scene_tree *parent);
void window_snapshot_geometry(struct window_snapshot *snapshot,struct wlr_box from,struct wlr_box to,double progress);
void window_snapshot_opacity(struct window_snapshot *snapshot,double visible);
void window_snapshot_apply(struct window_snapshot *snapshot,double visible,int offset_y);
size_t window_snapshot_count(struct window_snapshot *snapshot);
void window_snapshot_destroy(struct window_snapshot *snapshot);
struct window_hint {struct wlr_scene_tree *tree;struct wlr_scene_buffer *parts[5];};
bool window_hint_create(struct window_hint *hint,struct wlr_scene_tree *parent);
void window_hint_apply(struct window_hint *hint,struct wlr_box box,double progress);
#endif
