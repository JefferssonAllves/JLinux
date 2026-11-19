#ifndef JLINUX_DOCK_EFFECT_H
#define JLINUX_DOCK_EFFECT_H
#include "glass.h"
void jlinux_dock_material(cairo_t *cr, double outer_x, double outer_y, double inner_x, double inner_y, double focus, struct jlinux_glass effects);
void jlinux_dock_center_material(cairo_t *cr, struct jlinux_glass effects);
void jlinux_dock_edge_glow(cairo_t *cr, double strength);
void jlinux_dock_halo(cairo_t *cr, double x, double y, double radius, double strength);
#endif
