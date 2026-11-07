#ifndef JLINUX_FUTURISTA_H
#define JLINUX_FUTURISTA_H
#include "../effects/glass.h"
struct futurista {
    cairo_surface_t *wallpaper, *blurred, *firefox, *code;
    struct jlinux_glass glass;
    int selected, wifi, battery;
    double volume, brightness, hover[10];
    bool muted;
    char message[180];
};
bool futurista_init(struct futurista *scene, struct jlinux_glass glass);
void futurista_finish(struct futurista *scene);
void futurista_wallpaper(struct futurista *scene, cairo_t *cr, int width, int height);
void futurista_panel(struct futurista *scene, cairo_t *cr, int width);
void futurista_overlay(struct futurista *scene, cairo_t *cr, int width, int height, bool control_panel);
/* -1 outside, 0..9 ring, 10 center, 20.. control actions. */
int futurista_hit(double x, double y, int width, int height, bool control_panel);
struct ui_rect futurista_dock_bounds(int width, int height);
void futurista_status(struct futurista *scene);
#endif
