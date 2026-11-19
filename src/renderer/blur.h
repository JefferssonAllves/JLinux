#ifndef JLINUX_BLUR_H
#define JLINUX_BLUR_H
#include <cairo.h>
/* In-place box blur of an ARGB32 image; radius is bounded to 32 pixels. */
#include <stdbool.h>
bool jlinux_blur(cairo_surface_t *image, int radius);
#endif
