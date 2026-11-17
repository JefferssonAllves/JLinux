#ifndef JLINUX_RECORDING_H
#define JLINUX_RECORDING_H
#include <glib.h>
#include <stdbool.h>
#include <time.h>
struct recording { GPid pid; bool stopping; time_t started; char *path; };
bool recording_toggle(struct recording *recording, GError **error);
bool recording_complete(struct recording *recording, int status);
void recording_finish(struct recording *recording);
#endif
