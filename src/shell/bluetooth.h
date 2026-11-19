#ifndef JLINUX_BLUETOOTH_H
#define JLINUX_BLUETOOTH_H
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
struct bluetooth { pid_t pid; int fd, action; bool available, powered; struct timespec started; char error[120]; };
bool bluetooth_start(struct bluetooth *bt, int action);
void bluetooth_complete(struct bluetooth *bt, int status);
void bluetooth_finish(struct bluetooth *bt);
#endif
