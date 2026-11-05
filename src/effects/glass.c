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
    bool horizontal = rect.width > rect.height;
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
        cairo_pattern_add_color_stop_rgba(shine, 1, horizontal ? .50 : .66, horizontal ? .57 : .51, horizontal ? .76 : .81, horizontal ? .18 : .22);
        cairo_set_source(cr, shine); cairo_paint(cr); cairo_pattern_destroy(shine);
        /* Local light catches the bevel; it is not an animated reflection. */
        const double positions[] = {.18, .62, .93};
        for (int i = 0; i < 3; i++) {
            double x = rect.x + rect.width * positions[i];
            double y = i == 1 ? rect.y : rect.y + rect.height;
            double spread = rect.height > rect.width ? 90 : 125;
            if (rect.height > rect.width) { x = i == 1 ? rect.x : rect.x + rect.width; y = rect.y + rect.height * positions[i]; }
            cairo_pattern_t *light = cairo_pattern_create_radial(x, y, 0, x, y, spread);
            if (horizontal && i == 2) {
                cairo_pattern_add_color_stop_rgba(light, 0, .45, .60, 1, .32);
            } else {
                cairo_pattern_add_color_stop_rgba(light, 0, i == 1 ? .36 : .72, i == 1 ? .69 : .48, 1, .46);
            }
            cairo_pattern_add_color_stop_rgba(light, .24, .35, .34, .82, .14);
            cairo_pattern_add_color_stop_rgba(light, 1, .2, .25, .5, 0);
            cairo_set_source(cr, light); cairo_paint(cr); cairo_pattern_destroy(light);
        }
        cairo_pattern_t *bevel = cairo_pattern_create_linear(0, rect.y, 0, rect.y + rect.height);
        cairo_pattern_add_color_stop_rgba(bevel, 0, .88, .94, 1, .32);
        cairo_pattern_add_color_stop_rgba(bevel, .14, .63, .75, 1, .08);
        cairo_pattern_add_color_stop_rgba(bevel, .5, .33, .42, .66, .015);
        cairo_pattern_add_color_stop_rgba(bevel, .88, .64, .61, .87, .06);
        cairo_pattern_add_color_stop_rgba(bevel, 1, .79, .73, 1, .26);
        rounded(cr, rect, radius); cairo_set_source(cr, bevel);
        cairo_set_line_width(cr, 6); cairo_stroke(cr); cairo_pattern_destroy(bevel);
    }
    cairo_restore(cr);
    if (effects.reflection) {
        rounded(cr, rect, radius);
        cairo_pattern_t *edge = cairo_pattern_create_linear(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
        cairo_pattern_add_color_stop_rgba(edge, 0, .65, .86, 1, rect.width > rect.height ? .40 : .95);
        if (rect.width > rect.height) {
            cairo_pattern_add_color_stop_rgba(edge, 2 * radius / rect.width, .65, .86, 1, .95);
        }
        cairo_pattern_add_color_stop_rgba(edge, .3, .38, .64, 1, .68);
        cairo_pattern_add_color_stop_rgba(edge, .7, horizontal ? .48 : .66, horizontal ? .49 : .40, horizontal ? .83 : .95, .58);
        cairo_pattern_add_color_stop_rgba(edge, 1, horizontal ? .55 : .88, horizontal ? .67 : .61, horizontal ? .95 : 1, horizontal ? .70 : .98);
        cairo_set_source(cr, edge); cairo_set_line_width(cr, 1); cairo_stroke(cr);
        struct ui_rect inner = {rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4};
        rounded(cr, inner, radius - 2); cairo_set_source_rgba(cr, .75, .83, 1, .12);
        cairo_set_line_width(cr, 1); cairo_stroke(cr); cairo_pattern_destroy(edge);
    }
}
