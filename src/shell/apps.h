#ifndef JLINUX_SHELL_APPS_H
#define JLINUX_SHELL_APPS_H
#include <stdbool.h>
#include <cairo.h>
#include <glib.h>
const char *dock_app_name(int index);
cairo_surface_t *dock_app_icon(int index);
bool dock_app_launch(int index, GError **error);
#endif
