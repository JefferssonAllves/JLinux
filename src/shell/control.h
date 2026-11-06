#ifndef JLINUX_SHELL_CONTROL_H
#define JLINUX_SHELL_CONTROL_H
#include <stdbool.h>
int shell_control_open(void);
void shell_control_close(int fd);
bool shell_control_dock(bool visible);
#endif
