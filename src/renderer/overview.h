#ifndef JLINUX_OVERVIEW_H
#define JLINUX_OVERVIEW_H
#include "window-effects.h"
#define OVERVIEW_PAGE 9
struct wlr_output;
struct overview_card { struct wlr_scene_tree *tree; struct wlr_scene_buffer *frame; struct window_snapshot *snapshot; struct wlr_box box; };
struct window_overview {
    struct wlr_scene_tree *tree;
    struct wlr_box bounds;
    struct overview_card cards[OVERVIEW_PAGE];
    struct wlr_scene_buffer *header, *background;
    size_t selected;
    double progress;
    struct jlinux_glass glass;
    size_t count, page, total;
    bool blurred;
};
bool window_overview_create(struct window_overview *view,struct wlr_scene *scene,struct wlr_output *output,struct wlr_box bounds,struct jlinux_glass glass);
bool window_overview_page(struct window_overview *view,size_t total,size_t page);
bool window_overview_add(struct window_overview *view,size_t index,const char *title,struct wlr_scene_tree *source,int width,int height);
void window_overview_apply(struct window_overview *view,double progress);
void window_overview_select(struct window_overview *view,size_t index);
int window_overview_hit(struct window_overview *view,double x,double y);
void window_overview_finish(struct window_overview *view);
#endif
