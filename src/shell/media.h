#ifndef JLINUX_MEDIA_H
#define JLINUX_MEDIA_H
#include <gio/gio.h>
#include <stdbool.h>
struct media {
    GDBusConnection *bus;
    GCancellable *cancel;
    char *player;
    char title[180], artist[180];
    bool can_control, playing, can_play, can_pause, can_previous, can_next, dirty;
    guint properties, owners, pending, generation;
};
void media_init(struct media *media);
void media_action(struct media *media, int action);
void media_finish(struct media *media);
#endif
