#ifndef JLINUX_DOCK_H
#define JLINUX_DOCK_H
#include <gio/gdesktopappinfo.h>
struct dock_apps { GPtrArray *apps, *pins; gboolean writable; };
void dock_apps_init(struct dock_apps *dock);
void dock_apps_finish(struct dock_apps *dock);
gboolean dock_apps_pinned(const struct dock_apps *dock, GAppInfo *app);
gboolean dock_apps_toggle(struct dock_apps *dock, GAppInfo *app, GError **error);
#endif
