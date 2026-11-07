#include "dock.h"
void jlinux_dock_halo(cairo_t *cr, double x, double y, double radius, double strength) {
    if (strength <= 0 || radius <= 0) { return; }
    if (strength > 1) { strength = 1; }
    cairo_save(cr);
    cairo_pattern_t *glow = cairo_pattern_create_radial(x, y, radius * .2, x, y, radius);
    cairo_pattern_add_color_stop_rgba(glow, 0, .38, .67, 1, .24 * strength);
    cairo_pattern_add_color_stop_rgba(glow, .65, .65, .40, 1, .12 * strength);
    cairo_pattern_add_color_stop_rgba(glow, 1, .4, .4, 1, 0);
    cairo_set_source(cr, glow); cairo_arc(cr, x, y, radius, 0, 6.283185307179586); cairo_fill(cr);
    cairo_pattern_destroy(glow); cairo_restore(cr);
}
