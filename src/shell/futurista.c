#include "futurista.h"
#include "../renderer/blur.h"
#include "../effects/dock.h"
#include <gio/gio.h>
#include <glob.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static const double pi = 3.14159265358979323846;
struct png_reader {
    const unsigned char *bytes;
    size_t left;
};
static cairo_status_t read_png(void *data, unsigned char *out, unsigned int length) {
    struct png_reader *reader = data;
    if (length > reader->left) {
        return CAIRO_STATUS_READ_ERROR;
    }
    memcpy(out, reader->bytes, length);
    reader->bytes += length;
    reader->left -= length;
    return CAIRO_STATUS_SUCCESS;
}
static cairo_surface_t *asset(const char *name) {
    GBytes *bytes = g_resources_lookup_data(name, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    if (!bytes) {
        return NULL;
    }
    struct png_reader reader;
    reader.bytes = g_bytes_get_data(bytes, &reader.left);
    cairo_surface_t *image = cairo_image_surface_create_from_png_stream(read_png, &reader);
    g_bytes_unref(bytes);
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(image);
        return NULL;
    }
    return image;
}
bool futurista_init(struct futurista *s, struct jlinux_glass glass) {
    *s = (struct futurista){.glass = glass, .wifi = -1, .battery = -1, .volume = -1, .brightness = -1};
    s->hover[s->selected] = 1;
    s->wallpaper = asset("/org/jlinux/futurista/wallpaper.png");
    s->firefox = asset("/org/jlinux/futurista/firefox.png");
    s->code = asset("/org/jlinux/futurista/code.png");
    if (s->wallpaper && glass.blur) {
        s->blurred = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 418, 236);
        cairo_t *cr = cairo_create(s->blurred);
        futurista_wallpaper(s, cr, 418, 236);
        cairo_destroy(cr);
        if (!jlinux_blur(s->blurred, 7)) {
            return false;
        }
    }
    return s->wallpaper && s->firefox && s->code;
}
void futurista_finish(struct futurista *s) {
    if (s->blurred) {
        cairo_surface_destroy(s->blurred);
    }
    if (s->wallpaper) {
        cairo_surface_destroy(s->wallpaper);
    }
    if (s->firefox) {
        cairo_surface_destroy(s->firefox);
    }
    if (s->code) {
        cairo_surface_destroy(s->code);
    }
}
static void roundrect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -pi / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, pi / 2);
    cairo_arc(cr, x + r, y + h - r, r, pi / 2, pi);
    cairo_arc(cr, x + r, y + r, r, pi, pi * 1.5);
    cairo_close_path(cr);
}
static void image(cairo_t *cr, cairo_surface_t *img, double x, double y, double size) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / cairo_image_surface_get_width(img), size / cairo_image_surface_get_height(img));
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}
static void text(cairo_t *cr, const char *label, double x, double y, int size, bool bold, unsigned color) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_new();
    pango_font_description_set_family(font, "Adwaita Sans, Sans");
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    pango_font_description_set_weight(font, bold ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, label, -1);
    cairo_set_source_rgb(cr, ((color >> 16) & 255) / 255., ((color >> 8) & 255) / 255., (color & 255) / 255.);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);
}
static void material(cairo_t *cr, double x, double y, double w, double h, double r, bool active,
                     struct jlinux_glass effects) {
    if (effects.shadow) {
        roundrect(cr, x + 1, y + 3, w, h, r);
        cairo_set_source_rgba(cr, .006, .004, .025, .28);
        cairo_fill(cr);
    }
    bool panel = w > 450 && h > 500;
    roundrect(cr, x, y, w, h, r);
    cairo_pattern_t *p = cairo_pattern_create_linear(x, y, x + w, y + h);
    if (active && effects.tint) {
        cairo_pattern_add_color_stop_rgba(p, 0, .53, .25, 1, .98);
        cairo_pattern_add_color_stop_rgba(p, .5, w > 200 ? .51 : .35, w > 200 ? .27 : .20,
                                          w > 200 ? .97 : .83, .97);
        cairo_pattern_add_color_stop_rgba(p, 1, .50, .29, .96, .98);
    } else {
        cairo_pattern_add_color_stop_rgba(p, 0, effects.tint ? (panel ? .08 : .17) : .18,
                                          effects.tint ? (panel ? .10 : .19) : .18,
                                          effects.tint ? (panel ? .24 : .35) : .18, .97);
        cairo_pattern_add_color_stop_rgba(p, .42, effects.tint ? (panel ? .055 : .11) : .12,
                                          effects.tint ? (panel ? .07 : .14) : .12,
                                          effects.tint ? (panel ? .18 : .28) : .12, .97);
        cairo_pattern_add_color_stop_rgba(p, 1, effects.tint ? (panel ? .15 : .20) : .18,
                                          effects.tint ? (panel ? .10 : .18) : .18,
                                          effects.tint ? (panel ? .30 : .37) : .18, .97);
    }
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
    if (effects.reflection) {
        cairo_save(cr);
        cairo_clip_preserve(cr);
        cairo_pattern_t *light = cairo_pattern_create_linear(x, y, x, y + h);
        cairo_pattern_add_color_stop_rgba(light, 0, .85, .86, 1, active ? .30 : panel ? .10 : .12);
        cairo_pattern_add_color_stop_rgba(light, .15, .60, .62, 1, .035);
        cairo_pattern_add_color_stop_rgba(light, .7, .5, .4, .9, 0);
        cairo_pattern_add_color_stop_rgba(light, 1, .7, .3, 1, active ? .22 : .08);
        cairo_set_source(cr, light);
        cairo_paint(cr);
        cairo_pattern_destroy(light);
        cairo_restore(cr);
        p = cairo_pattern_create_linear(x, y, x, y + h);
        cairo_pattern_add_color_stop_rgba(p, 0, .70, .73, 1, active ? .95 : .42);
        cairo_pattern_add_color_stop_rgba(p, .4, .49, .43, .80, .16);
        cairo_pattern_add_color_stop_rgba(p, 1, .78, .35, 1, active || panel ? 1 : .50);
        if (panel || active) {
            cairo_set_source_rgba(cr, .69, .30, 1, .12);
            cairo_set_line_width(cr, panel ? 10 : 7);
            cairo_stroke_preserve(cr);
            cairo_set_source_rgba(cr, .77, .41, 1, .22);
            cairo_set_line_width(cr, 3);
            cairo_stroke_preserve(cr);
        }
        cairo_set_source(cr, p);
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);
        cairo_pattern_destroy(p);
    } else {
        cairo_new_path(cr);
    }
}
static void circle(cairo_t *cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * pi); }
static void line_icon(cairo_t *cr, int kind, double x, double y, double size, unsigned color) {
    if (kind == 0 || kind == 1 || kind == 10) {
        ui_icon(cr, kind == 0 ? UI_WIFI : kind == 1 ? UI_BLUETOOTH : UI_POWER, x, y, size, color);
        return;
    }
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / 32, size / 32);
    cairo_set_source_rgb(cr, ((color >> 16) & 255) / 255., ((color >> 8) & 255) / 255., (color & 255) / 255.);
    cairo_set_line_width(cr, 2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    switch (kind) {
    case 2:
        cairo_move_to(cr, 3, 14);
        cairo_line_to(cr, 13, 16);
        cairo_line_to(cr, 11, 4);
        cairo_line_to(cr, 14, 4);
        cairo_line_to(cr, 20, 16);
        cairo_line_to(cr, 29, 19);
        cairo_line_to(cr, 29, 22);
        cairo_line_to(cr, 19, 21);
        cairo_line_to(cr, 17, 29);
        cairo_line_to(cr, 14, 29);
        cairo_line_to(cr, 14, 21);
        cairo_line_to(cr, 3, 17);
        break;
    case 3:
        cairo_arc(cr, 17, 15, 12, .4, 4.4);
        cairo_curve_to(cr, 8, 19, 13, 26, 27, 20);
        break;
    case 4:
        cairo_move_to(cr, 6, 27);
        cairo_curve_to(cr, 5, 10, 12, 5, 27, 4);
        cairo_curve_to(cr, 28, 24, 19, 29, 8, 22);
        cairo_move_to(cr, 6, 28);
        cairo_line_to(cr, 23, 9);
        break;
    case 5:
        circle(cr, 16, 16, 6);
        for (int i = 0; i < 8; i++) {
            double a = i * pi / 4;
            cairo_move_to(cr, 16 + 11 * cos(a), 16 + 11 * sin(a));
            cairo_line_to(cr, 16 + 14 * cos(a), 16 + 14 * sin(a));
        }
        break;
    case 6:
        for (int i = 0; i < 4; i++) {
            cairo_save(cr);
            cairo_translate(cr, 16, 16);
            cairo_rotate(cr, i * pi / 2);
            cairo_move_to(cr, -12, -5);
            cairo_line_to(cr, -12, -12);
            cairo_line_to(cr, -5, -12);
            cairo_restore(cr);
        }
        break;
    case 7:
        cairo_rectangle(cr, 2, 4, 28, 20);
        cairo_move_to(cr, 16, 24);
        cairo_line_to(cr, 16, 29);
        cairo_move_to(cr, 9, 29);
        cairo_line_to(cr, 23, 29);
        break;
    case 8:
        cairo_move_to(cr, 15, 8);
        cairo_line_to(cr, 22, 8);
        cairo_line_to(cr, 29, 16);
        cairo_line_to(cr, 22, 24);
        cairo_line_to(cr, 15, 24);
        cairo_move_to(cr, 4, 6);
        cairo_line_to(cr, 9, 6);
        cairo_move_to(cr, 4, 6);
        cairo_line_to(cr, 4, 26);
        cairo_line_to(cr, 9, 26);
        break;
    case 9:
        cairo_arc(cr, 16, 16, 12, .2, 5.1);
        cairo_move_to(cr, 17, 2);
        cairo_line_to(cr, 23, 5);
        cairo_line_to(cr, 18, 10);
        break;
    case 11:
        cairo_move_to(cr, 3, 12);
        cairo_line_to(cr, 8, 12);
        cairo_line_to(cr, 15, 5);
        cairo_line_to(cr, 15, 27);
        cairo_line_to(cr, 8, 20);
        cairo_line_to(cr, 3, 20);
        cairo_close_path(cr);
        cairo_move_to(cr, 21, 8);
        cairo_curve_to(cr, 29, 10, 29, 22, 21, 25);
        cairo_move_to(cr, 20, 12);
        cairo_curve_to(cr, 23, 14, 23, 19, 20, 21);
        break;
    case 13:
        for (int i = 0; i < 3; i++) {
            double y = 6 + i * 10, knob = i == 1 ? 21 : 10;
            cairo_move_to(cr, 3, y);
            cairo_line_to(cr, knob - 3, y);
            cairo_move_to(cr, knob + 3, y);
            cairo_line_to(cr, 29, y);
            cairo_new_sub_path(cr);
            circle(cr, knob, y, 3);
        }
        break;
    case 12:
        circle(cr, 13, 13, 10);
        cairo_move_to(cr, 21, 21);
        cairo_line_to(cr, 29, 29);
        break;
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}
static void app_icon(struct futurista *s, cairo_t *cr, int index, double x, double y, double size) {
    if (index == 0) {
        image(cr, s->firefox, x, y, size);
        return;
    }
    if (index == 2) {
        image(cr, s->code, x, y, size);
        return;
    }
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / 80, size / 80);
    if (index == 1) {
        material(cr, 0, 0, 80, 76, 14, false, s->glass);
        cairo_set_source_rgb(cr, .035, .04, .055);
        roundrect(cr, 3, 3, 74, 70, 12);
        cairo_fill(cr);
        text(cr, "›_", 11, 4, 44, true, 0xffffff);
    } else if (index == 3) {
        cairo_set_source_rgb(cr, .02, .84, .26);
        circle(cr, 40, 40, 38);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0, .06, .03);
        cairo_set_line_width(cr, 6);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        for (int i = 0; i < 3; i++) {
            cairo_move_to(cr, 17 + i * 2, 29 + i * 12);
            cairo_curve_to(cr, 32, 22 + i * 12, 51, 24 + i * 12, 64 - i * 2, 34 + i * 10);
            cairo_stroke(cr);
        }
    } else if (index == 4) {
        cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 0, 78);
        cairo_pattern_add_color_stop_rgb(p, 0, .42, .84, 1);
        cairo_pattern_add_color_stop_rgb(p, 1, .04, .41, .96);
        roundrect(cr, 7, 12, 35, 20, 7);
        cairo_set_source(cr, p);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, .85, .95, 1);
        roundrect(cr, 6, 23, 67, 43, 5);
        cairo_fill(cr);
        roundrect(cr, 4, 30, 72, 47, 8);
        cairo_set_source(cr, p);
        cairo_fill(cr);
        cairo_pattern_destroy(p);
    } else if (index == 5) {
        cairo_set_source_rgb(cr, .66, .72, .88);
        cairo_translate(cr, 40, 40);
        for (int i = 0; i < 10; i++) {
            cairo_save(cr);
            cairo_rotate(cr, i * pi / 5);
            cairo_rectangle(cr, -6, -39, 12, 18);
            cairo_fill(cr);
            cairo_restore(cr);
        }
        circle(cr, 0, 0, 31);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, .25, .29, .47);
        circle(cr, 0, 0, 21);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, .7, .76, .9);
        circle(cr, 0, 0, 11);
        cairo_set_line_width(cr, 5);
        cairo_stroke(cr);
    } else if (index == 6) {
        material(cr, 3, 5, 74, 68, 14, true, s->glass);
        cairo_set_source_rgb(cr, .12, .62, 1);
        roundrect(cr, 5, 5, 70, 64, 12);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, .72, .27);
        circle(cr, 24, 22, 6);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, .84, .87, 1);
        cairo_move_to(cr, 8, 59);
        cairo_line_to(cr, 31, 31);
        cairo_line_to(cr, 56, 68);
        cairo_line_to(cr, 8, 68);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, .63, .80);
        cairo_move_to(cr, 35, 68);
        cairo_line_to(cr, 54, 41);
        cairo_line_to(cr, 72, 62);
        cairo_line_to(cr, 72, 68);
        cairo_fill(cr);
    } else if (index == 7 || index == 8) {
        material(cr, 4, 9, 72, 60, 12, index == 7, s->glass);
        if (index == 7) {
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_move_to(cr, 29, 24);
            cairo_line_to(cr, 29, 57);
            cairo_line_to(cr, 56, 41);
            cairo_close_path(cr);
            cairo_fill(cr);
        } else {
            text(cr, "‹/›", 12, 18, 34, true, 0xffffff);
        }
    } else if (index == 9) {
        cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 0, 80);
        cairo_pattern_add_color_stop_rgb(p, 0, .035, .07, .24);
        cairo_pattern_add_color_stop_rgb(p, 1, 0, .40, .70);
        circle(cr, 40, 40, 40);
        cairo_set_source(cr, p);
        cairo_fill(cr);
        cairo_pattern_destroy(p);
        cairo_set_source_rgb(cr, .95, .98, 1);
        cairo_set_line_width(cr, 7);
        cairo_move_to(cr, 8, 50);
        cairo_line_to(cr, 29, 60);
        cairo_line_to(cr, 57, 25);
        cairo_stroke(cr);
        circle(cr, 57, 24, 14);
        cairo_stroke(cr);
        circle(cr, 29, 59, 10);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}
void futurista_wallpaper(struct futurista *s, cairo_t *cr, int width, int height) {
    double iw = cairo_image_surface_get_width(s->wallpaper),
           ih = cairo_image_surface_get_height(s->wallpaper);
    double scale = fmax(width / iw, height / ih);
    cairo_save(cr);
    cairo_translate(cr, (width - iw * scale) / 2, (height - ih * scale) / 2);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, s->wallpaper, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}
static void blurred_backdrop(struct futurista *s, cairo_t *cr, int width, int height) {
    if (!s->blurred) {
        return;
    }
    cairo_save(cr);
    cairo_clip(cr);
    cairo_scale(cr, width / (double)cairo_image_surface_get_width(s->blurred),
                height / (double)cairo_image_surface_get_height(s->blurred));
    cairo_set_source_surface(cr, s->blurred, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}
void futurista_panel(struct futurista *s, cairo_t *cr, int width) {
    cairo_save(cr);
    roundrect(cr, 12, -4, width - 24, 56, 28);
    blurred_backdrop(s, cr, width, 941);
    roundrect(cr, 12, -4, width - 24, 56, 28);
    cairo_pattern_t *header = cairo_pattern_create_linear(0, 0, width, 52);
    cairo_pattern_add_color_stop_rgba(header, 0, s->glass.tint ? .045 : .07, s->glass.tint ? .055 : .07,
                                      s->glass.tint ? .13 : .07, .97);
    cairo_pattern_add_color_stop_rgba(header, .46, s->glass.tint ? .075 : .10, s->glass.tint ? .09 : .10,
                                      s->glass.tint ? .22 : .10, .97);
    cairo_pattern_add_color_stop_rgba(header, 1, s->glass.tint ? .08 : .10, s->glass.tint ? .09 : .10,
                                      s->glass.tint ? .22 : .10, .97);
    cairo_set_source(cr, header);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(header);
    if (s->glass.reflection) {
        cairo_set_source_rgba(cr, .48, .45, .90, .55);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
    } else {
        cairo_new_path(cr);
    }
    cairo_pattern_t *p = cairo_pattern_create_linear(42, 10, 72, 41);
    cairo_pattern_add_color_stop_rgb(p, 0, .84, .46, 1);
    cairo_pattern_add_color_stop_rgb(p, 1, .52, .28, .95);
    circle(cr, 57, 25, 15);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    cairo_set_source_rgb(cr, .10, .07, .24);
    cairo_move_to(cr, 48, 13);
    cairo_line_to(cr, 54, 14);
    cairo_line_to(cr, 58, 19);
    cairo_line_to(cr, 67, 18);
    cairo_line_to(cr, 69, 24);
    cairo_line_to(cr, 63, 26);
    cairo_line_to(cr, 59, 23);
    cairo_line_to(cr, 55, 28);
    cairo_line_to(cr, 55, 36);
    cairo_line_to(cr, 50, 31);
    cairo_line_to(cr, 51, 24);
    cairo_line_to(cr, 44, 21);
    cairo_close_path(cr);
    cairo_fill(cr);
    text(cr, "JLinux", 84, 12, 23, true, 0xf8f8ff);
    line_icon(cr, 11, width - 220, 13, 25, 0xf4f3ff);
    cairo_set_source_rgb(cr, .94, .94, 1);
    roundrect(cr, width - 176, 17, 27, 14, 2);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
    cairo_rectangle(cr, width - 147, 21, 3, 6);
    cairo_fill(cr);
    if (s->battery >= 0) {
        cairo_rectangle(cr, width - 173, 20, 21 * s->battery / 100., 8);
        cairo_fill(cr);
    }
    char battery[20];
    snprintf(battery, sizeof(battery), s->battery >= 0 ? "%d%%" : "—", s->battery);
    text(cr, battery, width - 137, 15, 18, false, 0xf1f2ff);
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char clock[80];
    static const char *days[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};
    static const char *months[] = {"Jan", "Fev", "Mar", "Abr", "Mai", "Jun",
                                   "Jul", "Ago", "Set", "Out", "Nov", "Dez"};
    snprintf(clock, sizeof(clock), "%s, %02d de %s  %02d:%02d", days[t.tm_wday], t.tm_mday, months[t.tm_mon],
             t.tm_hour, t.tm_min);
    PangoLayout *label = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_from_string("Adwaita Sans 18px");
    pango_layout_set_font_description(label, font);
    pango_layout_set_text(label, clock, -1);
    int clock_width;
    pango_layout_get_pixel_size(label, &clock_width, NULL);
    text(cr, clock, (width - clock_width) / 2., 15, 18, false, 0xf1f2ff);
    pango_font_description_free(font);
    g_object_unref(label);
    material(cr, width - 57, 7, 39, 38, 12, false, s->glass);
    line_icon(cr, 13, width - 50, 14, 24, 0xf1f2ff);
    cairo_restore(cr);
}
static void tint_stop(cairo_pattern_t *pattern, double position, double r, double g, double b, double alpha,
                      bool tint) {
    if (!tint) {
        r = g = b = (r + g + b) / 3;
    }
    cairo_pattern_add_color_stop_rgba(pattern, position, r, g, b, alpha);
}
static void sector(cairo_t *cr, int index, double focus, struct jlinux_glass effects) {
    double a = (-108 + index * 36 + .6) * pi / 180, b = (-72 + index * 36 - .6) * pi / 180;
    double outer = 326 + 4 * focus, inner = 176 - 11 * focus;
    const double corner = 5;
    cairo_new_path(cr);
    cairo_arc(cr, 836, 469, outer, a + corner / outer, b - corner / outer);
    cairo_curve_to(cr, 836 + outer * cos(b), 469 + outer * sin(b), 836 + outer * cos(b), 469 + outer * sin(b),
                   836 + (outer - corner) * cos(b), 469 + (outer - corner) * sin(b));
    cairo_line_to(cr, 836 + (inner + corner) * cos(b), 469 + (inner + corner) * sin(b));
    cairo_curve_to(cr, 836 + inner * cos(b), 469 + inner * sin(b), 836 + inner * cos(b), 469 + inner * sin(b),
                   836 + inner * cos(b - corner / inner), 469 + inner * sin(b - corner / inner));
    cairo_arc_negative(cr, 836, 469, inner, b - corner / inner, a + corner / inner);
    cairo_curve_to(cr, 836 + inner * cos(a), 469 + inner * sin(a), 836 + inner * cos(a), 469 + inner * sin(a),
                   836 + (inner + corner) * cos(a), 469 + (inner + corner) * sin(a));
    cairo_line_to(cr, 836 + (outer - corner) * cos(a), 469 + (outer - corner) * sin(a));
    cairo_curve_to(cr, 836 + outer * cos(a), 469 + outer * sin(a), 836 + outer * cos(a), 469 + outer * sin(a),
                   836 + outer * cos(a + corner / outer), 469 + outer * sin(a + corner / outer));
    cairo_close_path(cr);
    double mid = (a + b) / 2;
    cairo_pattern_t *p = cairo_pattern_create_linear(836 + outer * cos(mid), 469 + outer * sin(mid),
                                                     836 + inner * cos(mid), 469 + inner * sin(mid));
    tint_stop(p, 0, .42 + .04 * focus, .40 - .15 * focus, .66 + .30 * focus, .98, effects.tint);
    tint_stop(p, .5, .26 + .10 * focus, .25 - .07 * focus, .48 + .27 * focus, .98, effects.tint);
    tint_stop(p, 1, .18 + .37 * focus, .18 + .16 * focus, .36 + .60 * focus, .98, effects.tint);
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
    if (effects.reflection) {
        if (focus > 0) {
            cairo_set_source_rgba(cr, .70, .32, 1, .16 * focus);
            cairo_set_line_width(cr, 13);
            cairo_stroke_preserve(cr);
            cairo_set_source_rgba(cr, .82, .48, 1, .30 * focus);
            cairo_set_line_width(cr, 5);
            cairo_stroke_preserve(cr);
        }
        cairo_set_source_rgba(cr, .58 + .31 * focus, .61 + .04 * focus, 1, .75 + .25 * focus);
        cairo_set_line_width(cr, 1.3 + .7 * focus);
        cairo_stroke(cr);
    } else {
        cairo_new_path(cr);
    }
}
static void controls(struct futurista *s, cairo_t *cr, int width, int height) {
    material(cr, 1194, 66, 462, 760, 38, false, s->glass);
    cairo_set_source_rgba(cr, .85, .85, 1, .8);
    roundrect(cr, 1407, 83, 47, 6, 3);
    cairo_fill(cr);
    const char *labels[] = {"Wi-Fi", "Bluetooth", "Modo Avião"};
    const char *states[] = {s->wifi == 1   ? "Conectado"
                            : s->wifi == 0 ? "Desconectado"
                                           : "Indisponível",
                            "Indisponível", "Indisponível"};
    for (int i = 0; i < 3; i++) {
        double x = 1223 + i * 145;
        material(cr, x, 125, 124, 120, 26, i == 0 && s->wifi == 1, s->glass);
        line_icon(cr, i, x + 45, 143, 34, 0xffffff);
        text(cr, labels[i], x + (i == 2 ? 16 : 34), 194, 17, true, 0xffffff);
        text(cr, states[i], x + 12, 218, 14, false, 0xd8daee);
    }
    const char *small[] = {"Não Perturbe", "Economia", "Tela Noturna", "Captura"};
    const char *second[] = {"", "de Energia", "", "de Tela"};
    for (int i = 0; i < 4; i++) {
        double x = 1220 + i * 106;
        material(cr, x, 272, 92, 106, 24, false, s->glass);
        line_icon(cr, i + 3, x + 31, 289, 30, 0xf7f7ff);
        text(cr, small[i], x + 4, 340, 13, true, 0xffffff);
        text(cr, second[i], x + 11, 357, 12, true, 0xffffff);
    }
    for (int i = 0; i < 2; i++) {
        double y = 400 + i * 72;
        material(cr, 1220, y, 417, 65, 24, false, s->glass);
        line_icon(cr, i == 0 ? 5 : 11, 1236, y + 15, 32, 0xffffff);
        double value = i == 0 ? s->brightness : s->volume;
        double shown = value < 0 ? 0 : value;
        material(cr, 1287, y + 17, 339, 31, 16, false, s->glass);
        if (shown > 0) {
            material(cr, 1287, y + 17, 339 * fmin(shown, 1), 31, 16, true, s->glass);
        }
        cairo_set_source_rgb(cr, .97, .97, 1);
        circle(cr, 1287 + 339 * fmin(shown, 1), y + 32, 12);
        cairo_fill(cr);
        if (value < 0) {
            text(cr, i == 0 ? "Brilho indisponível" : "Áudio indisponível", 1320, y + 23, 12, false,
                 0xd7daed);
        }
    }
    cairo_set_source_rgba(cr, .7, .67, .9, .27);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 1224, 557);
    cairo_line_to(cr, 1635, 557);
    cairo_stroke(cr);
    text(cr, "Dispositivos", 1223, 573, 19, true, 0xffffff);
    material(cr, 1221, 602, 416, 76, 24, false, s->glass);
    line_icon(cr, 7, 1245, 624, 32, 0xffffff);
    text(cr, "Exibição", 1302, 621, 17, false, 0xffffff);
    char display[64];
    snprintf(display, sizeof(display), "%d × %d", width, height);
    text(cr, display, 1302, 646, 14, false, 0xd2d8ee);
    text(cr, "›", 1603, 621, 30, false, 0xffffff);
    const char *power[] = {"Encerrar", "Reiniciar", "Desligar"};
    for (int i = 0; i < 3; i++) {
        double x = 1222 + i * 145;
        material(cr, x, 702, 124, 101, 27, false, s->glass);
        line_icon(cr, 8 + i, x + 46, 720, 32, i == 2 ? 0xff779d : 0xffffff);
        text(cr, power[i], x + 25, 758, 14, true, 0xffffff);
        if (i == 0) {
            text(cr, "Sessão", x + 32, 777, 14, true, 0xffffff);
        }
    }
    if (s->message[0]) {
        cairo_save(cr);
        roundrect(cr, 1210, 832, 444, 74, 15);
        cairo_set_source_rgba(cr, .06, .04, .16, .97);
        cairo_fill(cr);
        cairo_rectangle(cr, 1220, 840, 425, 56);
        cairo_clip(cr);
        text(cr, s->message, 1224, 846, 13, false, 0xf8dfff);
        cairo_restore(cr);
    }
}
struct layout {
    double scale, x, y;
};
static struct layout layout(int width, int height) {
    double scale = fmin(width / 1672., fmax(1, height - 52) / 889.);
    return (struct layout){scale, (width - 1672 * scale) / 2,
                           52 + (height - 52 - 889 * scale) / 2 - 52 * scale};
}
struct ui_rect futurista_dock_bounds(int width,int height){
    struct layout l=layout(width,height);
    return (struct ui_rect){l.x+(836-345)*l.scale,l.y+(469-345)*l.scale,690*l.scale,690*l.scale};
}
void futurista_overlay(struct futurista *s, cairo_t *cr, int width, int height, bool control_panel) {
    struct layout l = layout(width, height);
    cairo_save(cr);
    cairo_translate(cr, l.x, l.y);
    cairo_scale(cr, l.scale, l.scale);
    if (control_panel) {
        roundrect(cr, 1194, 66, 462, 760, 38);
        blurred_backdrop(s, cr, 1672, 941);
        controls(s, cr, width, height);
        cairo_restore(cr);
        return;
    }
    if (s->blurred) {
        cairo_new_path(cr);
        circle(cr, 836, 469, 326);
        blurred_backdrop(s, cr, 1672, 941);
    }
    cairo_set_source_rgba(cr, .015, .045, .14, .50);
    circle(cr, 836, 469, 325);
    cairo_fill(cr);
    static const double positions[10][2] = {{836, 216}, {994, 270}, {1077, 407}, {1076, 552}, {983, 674},
                                            {837, 714}, {690, 674}, {597, 552},  {593, 405},  {674, 277}};
    for (int i = 0; i < 10; i++) {
        sector(cr, i, s->hover[i], s->glass);
        double size = (i == 0 ? 86 : i == 6 ? 68 : 74) * (1 + .10 * s->hover[i]);
        double y = positions[i][1] - 4 * s->hover[i];
        if (s->glass.reflection) { jlinux_dock_halo(cr, positions[i][0], y, size * .9, s->hover[i]); }
        app_icon(s, cr, i, positions[i][0] - size / 2, y - size / 2, size);
    }
    cairo_pattern_t *p = cairo_pattern_create_radial(820, 420, 20, 836, 469, 133);
    cairo_pattern_add_color_stop_rgba(p, 0, .32, .27, .62, .97);
    cairo_pattern_add_color_stop_rgba(p, .6, .25, .20, .50, .98);
    cairo_pattern_add_color_stop_rgba(p, 1, .08, .11, .26, .98);
    circle(cr, 836, 469, 132);
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
    if (s->glass.reflection) {
        cairo_set_source_rgba(cr, .67, .28, 1, .12);
        cairo_set_line_width(cr, 12);
        cairo_stroke_preserve(cr);
        cairo_set_source_rgba(cr, .78, .40, 1, .25);
        cairo_set_line_width(cr, 4);
        cairo_stroke_preserve(cr);
        cairo_set_source_rgba(cr, .70, .42, 1, .95);
        cairo_set_line_width(cr, 1.6);
        cairo_stroke(cr);
    } else {
        cairo_new_path(cr);
    }
    static const char *names[] = {
        "Firefox", "Terminal", "Visual Studio Code", "Spotify", "Arquivos", "Configurações",
        "Imagens", "Vídeos",   "Desenvolvimento",    "Steam"};
    app_icon(s, cr, s->selected, 788, 385, 96);
    text(cr, names[s->selected], s->selected == 2 ? 747 : 796, 491, s->selected == 2 ? 18 : 25, true,
         0xffffff);
    text(cr, s->selected == 0 ? "Navegador Web" : "Abrir aplicativo", 774, 525, 17, false, 0xb6b5d1);
    cairo_restore(cr);
}
int futurista_hit(double x, double y, int width, int height, bool control_panel) {
    struct layout l = layout(width, height);
    if (l.scale <= 0) {
        return -1;
    }
    x = (x - l.x) / l.scale;
    y = (y - l.y) / l.scale;
    if (!control_panel) {
        double dx = x - 836, dy = y - 469, r = hypot(dx, dy);
        if (r < 132) {
            return 10;
        }
        if (r >= 132) {
            double a = atan2(dy, dx) * 180 / pi + 108;
            while (a < 0) {
                a += 360;
            }
            return ((int)(a / 36)) % 10;
        }
        return -1;
    }
    if (x >= 1223 && x < 1637 && y >= 125 && y < 245) {
        return 20 + (int)((x - 1223) / 145);
    }
    if (x >= 1220 && x < 1637 && y >= 272 && y < 378) {
        return 23 + (int)((x - 1220) / 106);
    }
    if (x >= 1220 && x < 1280 && y >= 472 && y <= 537) {
        return 29;
    }
    if (x >= 1287 && x <= 1626 && y >= 472 && y <= 537) {
        return x < 1456 ? 30 : 31;
    }
    if (x >= 1221 && x < 1637 && y >= 602 && y < 678) {
        return 32;
    }
    if (x >= 1222 && x < 1637 && y >= 702 && y < 803) {
        return 33 + (int)((x - 1222) / 145);
    }
    if (x >= 1194 && x <= 1656 && y >= 66 && y <= 826) {
        return 40;
    }
    return -1;
}
static int read_number(const char *path) {
    FILE *f = fopen(path, "r");
    int n = -1;
    if (f) {
        if (fscanf(f, "%d", &n) != 1) {
            n = -1;
        }
        fclose(f);
    }
    return n;
}
void futurista_status(struct futurista *s) {
    glob_t paths = {0};
    s->battery = -1;
    s->wifi = -1;
    s->brightness = -1;
    if (!glob("/sys/class/backlight/*/brightness", 0, NULL, &paths) && paths.gl_pathc) {
        int value = read_number(paths.gl_pathv[0]);
        char maximum[512];
        snprintf(maximum, sizeof(maximum), "%.*s/max_brightness", (int)(strlen(paths.gl_pathv[0]) - 11),
                 paths.gl_pathv[0]);
        int max = read_number(maximum);
        if (value >= 0 && max > 0) {
            s->brightness = fmin(1, value / (double)max);
        }
    }
    globfree(&paths);
    memset(&paths, 0, sizeof(paths));
    if (!glob("/sys/class/power_supply/*/capacity", 0, NULL, &paths) && paths.gl_pathc) {
        int n = read_number(paths.gl_pathv[0]);
        if (n >= 0 && n <= 100) {
            s->battery = n;
        }
    }
    globfree(&paths);
    memset(&paths, 0, sizeof(paths));
    if (!glob("/sys/class/net/*/wireless", 0, NULL, &paths)) {
        s->wifi = 0;
        for (size_t i = 0; i < paths.gl_pathc; i++) {
            char path[512], state[24];
            snprintf(path, sizeof(path), "%.*s/operstate", (int)(strlen(paths.gl_pathv[i]) - 9),
                     paths.gl_pathv[i]);
            FILE *f = fopen(path, "r");
            if (f) {
                if (fscanf(f, "%23s", state) == 1 && !strcmp(state, "up")) {
                    s->wifi = 1;
                }
                fclose(f);
            }
        }
    }
    globfree(&paths);
}
