#ifndef JLINUX_VOLUME_H
#define JLINUX_VOLUME_H
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
struct volume { pid_t pid; int fd; bool query, available, muted; double level; struct timespec started; };
bool volume_start(struct volume *volume, int action);
void volume_complete(struct volume *volume, int status);
void volume_finish(struct volume *volume);
#endif
