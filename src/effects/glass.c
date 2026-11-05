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
        /* A cool translucent veil keeps glass readable even over a dark wallpaper. */
        cairo_set_source_rgba(cr, .34, .39, .49, .24); cairo_paint(cr);
    }
    if (effects.reflection) {
        cairo_pattern_t *shine = cairo_pattern_create_linear(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
        cairo_pattern_add_color_stop_rgba(shine, 0, .77, .85, 1, .34);
        cairo_pattern_add_color_stop_rgba(shine, .25, .45, .53, .69, .10);
        cairo_pattern_add_color_stop_rgba(shine, .75, .23, .21, .34, .035);
        cairo_pattern_add_color_stop_rgba(shine, 1, .66, .51, .81, .22);
        cairo_set_source(cr, shine); cairo_paint(cr); cairo_pattern_destroy(shine);
        /* Local light catches the bevel; it is not an animated reflection. */
        const double positions[] = {.18, .62, .93};
        for (int i = 0; i < 3; i++) {
            double x = rect.x + rect.width * positions[i];
            double y = i == 1 ? rect.y : rect.y + rect.height;
            double spread = rect.height > rect.width ? 90 : 125;
            if (rect.height > rect.width) { x = i == 1 ? rect.x : rect.x + rect.width; y = rect.y + rect.height * positions[i]; }
            cairo_pattern_t *light = cairo_pattern_create_radial(x, y, 0, x, y, spread);
            cairo_pattern_add_color_stop_rgba(light, 0, i == 1 ? .48 : .64, .54, 1, .35);
            cairo_pattern_add_color_stop_rgba(light, .24, .32, .32, .7, .10);
            cairo_pattern_add_color_stop_rgba(light, 1, .2, .25, .5, 0);
            cairo_set_source(cr, light); cairo_paint(cr); cairo_pattern_destroy(light);
        }
        rounded(cr, rect, radius); cairo_set_source_rgba(cr, .74, .81, 1, .07);
        cairo_set_line_width(cr, 10); cairo_stroke(cr);
        rounded(cr, rect, radius); cairo_set_source_rgba(cr, .81, .87, 1, .12);
        cairo_set_line_width(cr, 4); cairo_stroke(cr);
    }
    cairo_restore(cr);
    if (effects.reflection) {
        rounded(cr, rect, radius);
        cairo_pattern_t *edge = cairo_pattern_create_linear(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
        cairo_pattern_add_color_stop_rgba(edge, 0, .78, .89, 1, .8);
        cairo_pattern_add_color_stop_rgba(edge, .3, .55, .64, .9, .48);
        cairo_pattern_add_color_stop_rgba(edge, .7, .53, .43, .72, .35);
        cairo_pattern_add_color_stop_rgba(edge, 1, .82, .65, 1, .86);
        cairo_set_source(cr, edge); cairo_set_line_width(cr, 1); cairo_stroke(cr);
        struct ui_rect inner = {rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4};
        rounded(cr, inner, radius - 2); cairo_set_source_rgba(cr, .75, .83, 1, .18);
        cairo_set_line_width(cr, 1); cairo_stroke(cr); cairo_pattern_destroy(edge);
    }
}
