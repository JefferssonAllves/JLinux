#ifndef JLINUX_PAINTER_H
#define JLINUX_PAINTER_H
#include <stdbool.h>
#include <cairo.h>
struct ui_rect { double x, y, width, height; };
enum ui_icon { UI_HOME, UI_WIFI, UI_BLUETOOTH, UI_POWER, UI_FOLDER, UI_FILE, UI_UP, UI_REFRESH };
bool ui_contains(struct ui_rect rect, double x, double y);
void ui_finish(void);
void ui_wallpaper(cairo_t *cr, int width, int height);
void ui_text(cairo_t *cr, const char *text, double x, double y, int size, bool bold, unsigned int color);
void ui_icon(cairo_t *cr, enum ui_icon icon, double x, double y, double size, unsigned int color);
#endif
