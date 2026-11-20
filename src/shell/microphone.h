#ifndef JLINUX_MICROPHONE_H
#define JLINUX_MICROPHONE_H
#include <glib.h>
#include <stdbool.h>
struct microphone { GPid pid; bool playing, ready; char *directory, *path; gint64 started; };
bool microphone_start(struct microphone *mic, bool playback, GError **error);
bool microphone_complete(struct microphone *mic, int status);
void microphone_finish(struct microphone *mic);
#endif
