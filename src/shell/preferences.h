#ifndef JLINUX_SHELL_PREFERENCES_H
#define JLINUX_SHELL_PREFERENCES_H
#include <stdbool.h>
#include <glib.h>
struct shell_preferences { bool animations, zoom, neon, tint, reflection, shadow; };
struct shell_preferences shell_preferences_default(void);
bool shell_preferences_load(struct shell_preferences *prefs, GError **error);
bool shell_preferences_save(const struct shell_preferences *prefs, GError **error);
#endif
