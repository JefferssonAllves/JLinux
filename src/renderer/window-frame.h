#ifndef JLINUX_WINDOW_FRAME_H
#define JLINUX_WINDOW_FRAME_H
#include <wlr/types/wlr_scene.h>
#include "../effects/glass.h"
enum { JLINUX_FRAME_BORDER = 6, JLINUX_FRAME_HEADER = 36, JLINUX_FRAME_RADIUS = 6 };
struct jlinux_window_frame {
    struct wlr_scene_buffer *parts[4];
};
bool jlinux_window_frame_create(struct jlinux_window_frame *frame, struct wlr_scene_tree *parent);
bool jlinux_window_frame_update(struct jlinux_window_frame *frame, int width, int height,
    const char *title, struct jlinux_glass effects);
void jlinux_window_frame_focus(struct jlinux_window_frame *frame, bool focused);
#endif
