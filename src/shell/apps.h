#ifndef JLINUX_SHELL_APPS_H
#define JLINUX_SHELL_APPS_H
#include <stdbool.h>
#include <cairo.h>
#include <glib.h>
enum { DOCK_APP_COUNT = 10 };
const char *dock_app_name(int index);
const char *dock_app_desktop(int index);
const char *dock_app_icon_name(int index);
bool dock_app_matches(int index,const char *app_id,const char *title);
cairo_surface_t *dock_app_icon(int index);
bool dock_app_launch(int index, GError **error);
#endif
