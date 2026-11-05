#ifndef JLINUX_GLASS_H
#define JLINUX_GLASS_H
#include "../toolkit/painter.h"
struct jlinux_glass { bool blur, tint, reflection, shadow; };
void jlinux_glass_window_shading(cairo_t *cr, struct ui_rect rect, double radius);
void jlinux_glass_window_reflections(cairo_t *cr, struct ui_rect rect, double radius, double header);
bool jlinux_glass_settings(struct jlinux_glass *effects);
void jlinux_glass_paint(cairo_t *cr, struct ui_rect rect, double radius,
    cairo_surface_t *backdrop, double origin_x, double origin_y,
    struct jlinux_glass effects);
#endif
