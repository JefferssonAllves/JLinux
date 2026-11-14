#include "glass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void rounded(cairo_t *cr, struct ui_rect r, double radius) {
    const double pi = 3.14159265358979323846;
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.width - radius, r.y + radius, radius, -pi / 2, 0);
    cairo_arc(cr, r.x + r.width - radius, r.y + r.height - radius, radius, 0, pi / 2);
    cairo_arc(cr, r.x + radius, r.y + r.height - radius, radius, pi / 2, pi);
    cairo_arc(cr, r.x + radius, r.y + radius, radius, pi, pi * 1.5); cairo_close_path(cr);
}
static cairo_pattern_t *rim_colors(struct ui_rect rect, double radius, bool horizontal) {
    cairo_pattern_t *edge = cairo_pattern_create_linear(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
    cairo_pattern_add_color_stop_rgba(edge, 0, .30, .72, 1, horizontal ? .55 : 1);
    if (horizontal) {
        cairo_pattern_add_color_stop_rgba(edge, 2 * radius / rect.width, .30, .72, 1, 1);
    }
    cairo_pattern_add_color_stop_rgba(edge, .3, .18, .51, 1, .88);
    cairo_pattern_add_color_stop_rgba(edge, .7, horizontal ? .42 : .62, horizontal ? .28 : .20, 1, .82);
    cairo_pattern_add_color_stop_rgba(edge, 1, horizontal ? .32 : .72, horizontal ? .48 : .34, 1, .95);
    return edge;
}
void jlinux_glass_paint(cairo_t *cr, struct ui_rect rect, double radius,
        cairo_surface_t *backdrop, double origin_x, double origin_y, struct jlinux_glass effects) {
    bool horizontal = rect.width > rect.height;
    bool thin_rim = horizontal && rect.height < 60;
    cairo_save(cr); rounded(cr, rect, radius); cairo_clip(cr);
    if (backdrop) {
        cairo_save(cr); cairo_scale(cr, 4, 4);
        cairo_set_source_surface(cr, backdrop, -origin_x / 4, -origin_y / 4);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
        cairo_paint(cr); cairo_restore(cr);
    }
    if (effects.tint) {
        /* Dark blue tint separates surfaces from their brighter chromatic rims. */
        cairo_set_source_rgba(cr, .055, .075, .14, .44); cairo_paint(cr);
    }
    if (effects.reflection) {
        cairo_pattern_t *shine = cairo_pattern_create_linear(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
        cairo_pattern_add_color_stop_rgba(shine, 0, .58, .70, 1, .20);
        cairo_pattern_add_color_stop_rgba(shine, .25, .24, .32, .55, .07);
        cairo_pattern_add_color_stop_rgba(shine, .75, .23, .21, .34, .035);
        cairo_pattern_add_color_stop_rgba(shine, 1, horizontal ? .30 : .46, horizontal ? .37 : .31, .72, horizontal ? .12 : .16);
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
        cairo_set_line_width(cr, thin_rim ? 2 : 6); cairo_stroke(cr); cairo_pattern_destroy(bevel);
        if (horizontal && rect.height < 60) {
            for (int i = 0; i < 3; i++) {
                cairo_save(cr);
                cairo_translate(cr, rect.x + rect.width * positions[i],
                    i == 1 ? rect.y + 1 : rect.y + rect.height - 1);
                cairo_scale(cr, 1, .10);
                cairo_pattern_t *glint = cairo_pattern_create_radial(0, 0, 0, 0, 0, 52);
                cairo_pattern_add_color_stop_rgba(glint, 0, .93, .97, 1, .92);
                cairo_pattern_add_color_stop_rgba(glint, .09, .68, .80, 1, .55);
                cairo_pattern_add_color_stop_rgba(glint, .4, .46, .52, .94, .19);
                cairo_pattern_add_color_stop_rgba(glint, 1, .4, .4, .8, 0);
                cairo_set_source(cr, glint); cairo_paint(cr); cairo_pattern_destroy(glint);
                cairo_restore(cr);
            }
        }
    }
    cairo_restore(cr);
    if (effects.reflection) {
        rounded(cr, rect, radius);
        cairo_pattern_t *edge = rim_colors(rect, radius, horizontal);
        cairo_set_source(cr, edge); cairo_set_line_width(cr, thin_rim ? 1.1 : 1.6); cairo_stroke(cr);
        if (!thin_rim) {
            struct ui_rect inner = {rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4};
            rounded(cr, inner, radius - 2); cairo_set_source_rgba(cr, .75, .83, 1, .12);
            cairo_set_line_width(cr, 1); cairo_stroke(cr);
        }
        cairo_pattern_destroy(edge);
    }
}

void jlinux_glass_panel_paint(cairo_t *cr, double width, double body_height, double fade_height,
        cairo_surface_t *backdrop, struct jlinux_glass effects) {
    double height = body_height + fade_height;
    cairo_save(cr); cairo_rectangle(cr, 0, 0, width, height); cairo_clip(cr);
    cairo_push_group(cr);
    /* Keep every material edge outside the panel; only the alpha ramp ends it. */
    jlinux_glass_paint(cr, (struct ui_rect){-6, -6, width + 12, height + 32}, 0,
        backdrop, 0, 0, effects);
    cairo_pop_group_to_source(cr);
    cairo_pattern_t *fade = cairo_pattern_create_linear(0, body_height, 0, height);
    cairo_pattern_add_color_stop_rgba(fade, 0, 1, 1, 1, 1);
    cairo_pattern_add_color_stop_rgba(fade, 1, 1, 1, 1, 0);
    cairo_mask(cr, fade); cairo_pattern_destroy(fade);
    cairo_restore(cr);
}

bool jlinux_glass_settings(struct jlinux_glass *effects) {
    const char *names[] = {"JLINUX_GLASS_BLUR", "JLINUX_GLASS_TINT", "JLINUX_GLASS_REFLECTION", "JLINUX_GLASS_SHADOW"};
    bool *values[] = {&effects->blur, &effects->tint, &effects->reflection, &effects->shadow};
    for (int i = 0; i < 4; i++) {
        const char *value = getenv(names[i]);
        if (value && strcmp(value, "0") && strcmp(value, "1")) {
            fprintf(stderr, "%s deve ser 0 ou 1.\n", names[i]); return false;
        }
        *values[i] = !value || !strcmp(value, "1");
    }
    return true;
}

void jlinux_glass_window_tint(cairo_t *cr, struct ui_rect rect, double radius) {
    cairo_save(cr); rounded(cr, rect, radius);
    cairo_set_source_rgba(cr, .012, .018, .045, .64); cairo_fill(cr);
    cairo_restore(cr);
}

void jlinux_glass_window_reflections(cairo_t *cr, struct ui_rect rect, double radius, double header) {
    cairo_save(cr); rounded(cr, rect, radius); cairo_clip(cr);
    cairo_pattern_t *sheen = cairo_pattern_create_linear(0, rect.y, 0, rect.y + header);
    cairo_pattern_add_color_stop_rgba(sheen, 0, .20, .55, 1, .108);
    cairo_pattern_add_color_stop_rgba(sheen, .18, .12, .32, .78, .0315);
    cairo_pattern_add_color_stop_rgba(sheen, .52, .02, .04, .09, .08);
    cairo_pattern_add_color_stop_rgba(sheen, .86, .45, .16, .78, .0405);
    cairo_pattern_add_color_stop_rgba(sheen, 1, .58, .32, 1, .09);
    cairo_save(cr); cairo_rectangle(cr, rect.x, rect.y, rect.width, header); cairo_clip(cr);
    cairo_set_source(cr, sheen); cairo_paint(cr); cairo_restore(cr); cairo_pattern_destroy(sheen);
    rounded(cr, rect, radius);
    jlinux_glass_rim(cr, rect, 1);
    cairo_new_path(cr);
    cairo_restore(cr);
}

void jlinux_glass_window_shading(cairo_t *cr, struct ui_rect rect, double radius) {
    cairo_save(cr); rounded(cr, rect, radius); cairo_clip(cr);
    struct ui_rect inset = {rect.x + 1.7, rect.y + 1.7, rect.width - 3.4, rect.height - 3.4};
    cairo_pattern_t *shade = cairo_pattern_create_linear(rect.x, rect.y, rect.x, rect.y + rect.height);
    cairo_pattern_add_color_stop_rgba(shade, 0, .005, .01, .025, .22);
    cairo_pattern_add_color_stop_rgba(shade, .5, .005, .01, .025, .30);
    cairo_pattern_add_color_stop_rgba(shade, 1, .005, .01, .025, .38);
    rounded(cr, inset, radius - 1.7); cairo_set_source(cr, shade);
    cairo_set_line_width(cr, 1.2); cairo_stroke(cr); cairo_pattern_destroy(shade);
    cairo_restore(cr);
}

void jlinux_glass_rim(cairo_t *cr, struct ui_rect rect, double strength) {
    for (int pass = 0; pass < 2; pass++) {
        cairo_pattern_t *edge = cairo_pattern_create_linear(rect.x, rect.y,
            rect.x + rect.width * .75, rect.y + rect.height);
        double alpha = (pass ? 1 : .18) * strength;
        cairo_pattern_add_color_stop_rgba(edge, 0, .08, .82, 1, alpha);
        cairo_pattern_add_color_stop_rgba(edge, .32, .08, .52, 1, alpha);
        cairo_pattern_add_color_stop_rgba(edge, .66, .84, .24, .91, alpha);
        cairo_pattern_add_color_stop_rgba(edge, 1, .24, .64, 1, alpha);
        cairo_set_source(cr, edge);
        cairo_set_line_width(cr, pass ? 1.6 : 2.2); cairo_stroke_preserve(cr);
        cairo_pattern_destroy(edge);
    }
}
