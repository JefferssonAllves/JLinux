#ifndef JLINUX_THUMBNAILS_H
#define JLINUX_THUMBNAILS_H
#include <cairo.h>
#include <glib.h>
/* Bounded cache; callers retain ownership of the returned surface. */
cairo_surface_t *file_thumbnail(GHashTable *cache, const char *path);
#endif
