#include "glass.h"
static void rounded(cairo_t *cr, struct ui_rect r, double radius) {
    const double pi = 3.14159265358979323846;
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.width - radius, r.y + radius, radius, -pi / 2, 0);
    cairo_arc(cr, r.x + r.width - radius, r.y + r.height - radius, radius, 0, pi / 2);
    cairo_arc(cr, r.x + radius, r.y + r.height - radius, radius, pi / 2, pi);
    cairo_arc(cr, r.x + radius, r.y + radius, radius, pi, pi * 1.5); cairo_close_path(cr);
}
void jlinux_glass_paint(cairo_t *cr, struct ui_rect rect, double radius,
        cairo_surface_t *backdrop, double origin_x, double origin_y, struct jlinux_glass effects) {
    cairo_save(cr); rounded(cr, rect, radius); cairo_clip(cr);
    if (backdrop) {
        cairo_save(cr); cairo_scale(cr, 4, 4);
        cairo_set_source_surface(cr, backdrop, -origin_x / 4, -origin_y / 4);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_paint(cr); cairo_restore(cr);
    }
    if (effects.tint) {
        cairo_set_source_rgba(cr, .018, .028, .055, .32); cairo_paint(cr);
    }
    if (effects.reflection) {
        cairo_pattern_t *shine = cairo_pattern_create_linear(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
        cairo_pattern_add_color_stop_rgba(shine, 0, .66, .81, 1, .23);
        cairo_pattern_add_color_stop_rgba(shine, .25, .34, .48, .77, .035);
        cairo_pattern_add_color_stop_rgba(shine, .75, .28, .19, .42, .025);
        cairo_pattern_add_color_stop_rgba(shine, 1, .66, .43, .8, .14);
        cairo_set_source(cr, shine); cairo_paint(cr); cairo_pattern_destroy(shine);
    }
    cairo_restore(cr);
    if (effects.reflection) {
        rounded(cr, rect, radius);
        cairo_pattern_t *edge = cairo_pattern_create_linear(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
        cairo_pattern_add_color_stop_rgba(edge, 0, .78, .89, 1, .8);
        cairo_pattern_add_color_stop_rgba(edge, .3, .39, .56, .8, .25);
        cairo_pattern_add_color_stop_rgba(edge, .7, .32, .25, .5, .18);
        cairo_pattern_add_color_stop_rgba(edge, 1, .73, .49, .83, .65);
        cairo_set_source(cr, edge); cairo_set_line_width(cr, 1); cairo_stroke(cr);
        struct ui_rect inner = {rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4};
        rounded(cr, inner, radius - 2); cairo_set_source_rgba(cr, .66, .78, 1, .09);
        cairo_set_line_width(cr, 1); cairo_stroke(cr); cairo_pattern_destroy(edge);
    }
}
