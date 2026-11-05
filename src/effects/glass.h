#ifndef JLINUX_GLASS_H
#define JLINUX_GLASS_H
#include "../toolkit/painter.h"
struct jlinux_glass { bool blur, tint, reflection; };
void jlinux_glass_paint(cairo_t *cr, struct ui_rect rect, double radius,
    cairo_surface_t *backdrop, double origin_x, double origin_y,
    struct jlinux_glass effects);
#endif
