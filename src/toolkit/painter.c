#include "painter.h"
#include <math.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>
static void color(cairo_t *cr, unsigned int value, double alpha) {
    cairo_set_source_rgba(cr, ((value >> 16) & 255) / 255.0,
        ((value >> 8) & 255) / 255.0, (value & 255) / 255.0, alpha);
}
bool ui_contains(struct ui_rect rect, double x, double y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}
static void round_rect(cairo_t *cr, struct ui_rect r, double radius) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.width - radius, r.y + radius, radius, -3.14159265358979323846 / 2, 0);
    cairo_arc(cr, r.x + r.width - radius, r.y + r.height - radius, radius, 0, 3.14159265358979323846 / 2);
    cairo_arc(cr, r.x + radius, r.y + r.height - radius, radius, 3.14159265358979323846 / 2, 3.14159265358979323846);
    cairo_arc(cr, r.x + radius, r.y + radius, radius, 3.14159265358979323846, 3.14159265358979323846 * 1.5);
    cairo_close_path(cr);
}
void ui_text(cairo_t *cr, const char *text, double x, double y, int size, bool bold, unsigned int value) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_new();
    pango_font_description_set_family(font, "Sans");
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    pango_font_description_set_weight(font, bold ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, text, -1);
    cairo_move_to(cr, x, y); color(cr, value, 1); pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font); g_object_unref(layout);
}
void ui_icon(cairo_t *cr, enum ui_icon icon, double x, double y, double size, unsigned int value) {
    cairo_new_path(cr); cairo_save(cr); cairo_translate(cr, x, y); cairo_scale(cr, size / 24, size / 24);
    color(cr, value, 1); cairo_set_line_width(cr, 1.7); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    switch (icon) {
    case UI_FOLDER:
        cairo_move_to(cr, 2, 7); cairo_line_to(cr, 2, 4); cairo_line_to(cr, 9, 4);
        cairo_line_to(cr, 12, 7); cairo_line_to(cr, 22, 7); cairo_line_to(cr, 22, 20);
        cairo_line_to(cr, 2, 20); cairo_close_path(cr); break;
    case UI_FILE:
        cairo_move_to(cr, 5, 2); cairo_line_to(cr, 14, 2); cairo_line_to(cr, 20, 8);
        cairo_line_to(cr, 20, 22); cairo_line_to(cr, 5, 22); cairo_close_path(cr);
        cairo_move_to(cr, 14, 2); cairo_line_to(cr, 14, 8); cairo_line_to(cr, 20, 8); break;
    case UI_UP:
        cairo_move_to(cr, 5, 11); cairo_line_to(cr, 12, 4); cairo_line_to(cr, 19, 11);
        cairo_move_to(cr, 12, 4); cairo_line_to(cr, 12, 21); break;
    case UI_REFRESH:
        cairo_arc(cr, 12, 12, 8, .4, 5.6); cairo_move_to(cr, 20, 3);
        cairo_line_to(cr, 20, 9); cairo_line_to(cr, 14, 9); break;
    case UI_HOME:
        cairo_move_to(cr, 3, 11); cairo_line_to(cr, 12, 3); cairo_line_to(cr, 21, 11);
        cairo_move_to(cr, 6, 10); cairo_line_to(cr, 6, 21); cairo_line_to(cr, 10, 21);
        cairo_line_to(cr, 10, 15); cairo_line_to(cr, 14, 15); cairo_line_to(cr, 14, 21);
        cairo_line_to(cr, 18, 21); cairo_line_to(cr, 18, 10); break;
    case UI_TERMINAL:
        cairo_rectangle(cr, 2, 3, 20, 18); cairo_move_to(cr, 6, 8); cairo_line_to(cr, 10, 12);
        cairo_line_to(cr, 6, 16); cairo_move_to(cr, 13, 16); cairo_line_to(cr, 18, 16); break;
    case UI_CODE:
        cairo_move_to(cr, 8, 6); cairo_line_to(cr, 2, 12); cairo_line_to(cr, 8, 18);
        cairo_move_to(cr, 16, 6); cairo_line_to(cr, 22, 12); cairo_line_to(cr, 16, 18);
        cairo_move_to(cr, 14, 3); cairo_line_to(cr, 10, 21); break;
    case UI_WIFI:
        cairo_arc(cr, 12, 22, 20, -2.16, -.98); cairo_new_sub_path(cr);
        cairo_arc(cr, 12, 22, 13, -2.16, -.98); cairo_new_sub_path(cr);
        cairo_arc(cr, 12, 22, 6, -2.16, -.98); cairo_new_sub_path(cr);
        cairo_arc(cr, 12, 20, 1, 0, 2 * 3.14159265358979323846); break;
    case UI_BLUETOOTH:
        cairo_move_to(cr, 11, 2); cairo_line_to(cr, 18, 7); cairo_line_to(cr, 5, 17);
        cairo_move_to(cr, 11, 2); cairo_line_to(cr, 11, 22); cairo_line_to(cr, 18, 17);
        cairo_line_to(cr, 5, 7); break;
    case UI_POWER:
        cairo_arc(cr, 12, 13, 9, -1.05, 3.14159265358979323846 + 1.05);
        cairo_move_to(cr, 12, 2); cairo_line_to(cr, 12, 12); break;
    }
    cairo_stroke(cr); cairo_restore(cr);
}
void ui_button(cairo_t *cr, struct ui_rect rect, enum ui_icon icon, bool hovered, unsigned int accent) {
    round_rect(cr, rect, 12);
    color(cr, hovered ? 0x647392 : 0x0c101b, hovered ? .38 : .16); cairo_fill(cr);
    ui_icon(cr, icon, rect.x + (rect.width - 26) / 2, rect.y + (rect.height - 26) / 2, 26, accent);
}

void ui_finish(void) {
    pango_cairo_font_map_set_default(NULL);
    cairo_debug_reset_static_data();
    FcFini();
}

void ui_wallpaper(cairo_t *cr, int width, int height) {
    cairo_save(cr); cairo_scale(cr, width / 1600.0, height / 900.0);
    cairo_pattern_t *sky = cairo_pattern_create_linear(0, 0, 1600, 900);
    cairo_pattern_add_color_stop_rgb(sky, 0, .015, .025, .06);
    cairo_pattern_add_color_stop_rgb(sky, .55, .08, .16, .25);
    cairo_pattern_add_color_stop_rgb(sky, 1, .015, .025, .065);
    cairo_set_source(cr, sky); cairo_paint(cr); cairo_pattern_destroy(sky);
    for (int i = 0; i < 3; i++) {
        cairo_pattern_t *glow = cairo_pattern_create_radial(420 + i * 430, 220 + i * 110, 0,
            420 + i * 430, 220 + i * 110, 470);
        cairo_pattern_add_color_stop_rgba(glow, 0, .12 + i * .04, .27, .55, .34);
        cairo_pattern_add_color_stop_rgba(glow, 1, .04, .09, .2, 0);
        cairo_set_source(cr, glow); cairo_paint(cr); cairo_pattern_destroy(glow);
    }
    for (int i = 0; i < 210; i++) {
        double x = (i * 337 % 1600), y = (i * 173 % 560);
        color(cr, 0xd7e9ff, .12 + (i % 5) * .09);
        cairo_arc(cr, x, y, i % 7 == 0 ? 1.2 : .6, 0, 6.283185307179586); cairo_fill(cr);
    }
    cairo_pattern_t *moon = cairo_pattern_create_radial(1120, 150, 3, 1160, 190, 95);
    cairo_pattern_add_color_stop_rgb(moon, 0, .76, .87, .95);
    cairo_pattern_add_color_stop_rgb(moon, 1, .22, .37, .53);
    cairo_set_source(cr, moon); cairo_arc(cr, 1160, 190, 95, 0, 6.283185307179586); cairo_fill(cr);
    cairo_pattern_destroy(moon);
    for (int layer = 0; layer < 4; layer++) {
        cairo_move_to(cr, -10, 900);
        for (int i = 0; i <= 32; i++) {
            double x = i * 51.0;
            double y = 490 + layer * 78 + sin(i * .57 + layer * 2) * (80 - layer * 12)
                + sin(i * 1.7 + layer) * 25;
            cairo_line_to(cr, x, y);
        }
        cairo_line_to(cr, 1610, 900); cairo_close_path(cr);
        color(cr, layer == 0 ? 0x1b344e : layer == 1 ? 0x132b40 : layer == 2 ? 0x101e32 : 0x080e1b, 1);
        cairo_fill(cr);
    }
    /* Static ripples give the glass a detailed backdrop without continuous redraw. */
    for (int i = 0; i < 60; i++) {
        double y = 660 + i * 4, x = 820 + sin(i * 2.7) * (20 + i * 4);
        color(cr, i % 3 ? 0x639fce : 0xa778bb, .08 + (i % 4) * .025);
        cairo_set_line_width(cr, 1); cairo_move_to(cr, x, y);
        cairo_line_to(cr, x + 25 + (i * 37 % 170), y); cairo_stroke(cr);
    }
    cairo_restore(cr);
}
