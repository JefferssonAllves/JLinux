#ifndef JLINUX_BACKDROP_H
#define JLINUX_BACKDROP_H
#include <stdbool.h>
struct wlr_scene_buffer;
struct wlr_scene;
struct jlinux_renderer;
/* Attach below a translucent native buffer. Lifetime follows that buffer. */
bool jlinux_backdrop_attach(struct wlr_scene_buffer *target);
void jlinux_backdrop_update(struct wlr_scene *scene,struct jlinux_renderer *renderer);
#endif
