#ifndef JLINUX_WINDOW_EFFECT_H
#define JLINUX_WINDOW_EFFECT_H
#include "glass.h"
enum { WINDOW_AURA_PAD = 20 };
void window_aura_paint(cairo_t *cr, struct ui_rect window, bool shadow, bool tint);
#endif
