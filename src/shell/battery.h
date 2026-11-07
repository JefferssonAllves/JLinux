#ifndef JLINUX_BATTERY_H
#define JLINUX_BATTERY_H
#include <stdbool.h>
struct battery_status { int percent; bool charging; };
struct battery_status battery_read(const char *root);
#endif
