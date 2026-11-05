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
void ui_panel(cairo_t *cr, struct ui_rect rect, double radius) {
    round_rect(cr, rect, radius);
    cairo_pattern_t *gradient = cairo_pattern_create_linear(0, rect.y, 0, rect.y + rect.height);
    cairo_pattern_add_color_stop_rgba(gradient, 0, .055, .075, .13, .96);
    cairo_pattern_add_color_stop_rgba(gradient, 1, .018, .025, .055, .96);
    cairo_set_source(cr, gradient); cairo_fill_preserve(cr); cairo_pattern_destroy(gradient);
    cairo_set_line_width(cr, 1); color(cr, 0x7c84b8, .65); cairo_stroke(cr);
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
    color(cr, hovered ? 0x26335a : 0x141d32, hovered ? .96 : .7); cairo_fill(cr);
    ui_icon(cr, icon, rect.x + (rect.width - 26) / 2, rect.y + (rect.height - 26) / 2, 26, accent);
}

void ui_finish(void) {
    pango_cairo_font_map_set_default(NULL);
    cairo_debug_reset_static_data();
    FcFini();
}
