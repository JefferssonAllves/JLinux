#ifndef JLINUX_FUTURISTA_H
#define JLINUX_FUTURISTA_H
#include "../effects/glass.h"
#include "preferences.h"
#include "media.h"
enum { FUTURISTA_PANEL_HEIGHT = 40 };
struct futurista {
    int bluetooth;
    bool bluetooth_busy;
    cairo_surface_t *wallpaper, *blurred, *icons[11];
    struct jlinux_glass glass;
    struct media *media;
    int selected, wifi, battery, settings_category;
    bool settings_open;
    struct shell_preferences prefs;
    double volume, brightness, hover[11];
    bool muted, charging;
    char message[180];
};
bool futurista_init(struct futurista *scene, struct jlinux_glass glass);
void futurista_finish(struct futurista *scene);
void futurista_wallpaper(struct futurista *scene, cairo_t *cr, int width, int height);
int futurista_media_hit(double x, double y, int width);
void futurista_panel(struct futurista *scene, cairo_t *cr, int width);
void futurista_overlay(struct futurista *scene, cairo_t *cr, int width, int height, bool control_panel);
/* -1 outside, 0..9 ring, 10 center, 20.. control actions. */
double futurista_volume_at(double x, int width, int height);
int futurista_settings_hit(double x, double y, int width, int height, int category);
int futurista_hit(double x, double y, int width, int height, bool control_panel);
struct ui_rect futurista_dock_bounds(int width, int height);
void futurista_status(struct futurista *scene);
#endif
