#ifndef JLINUX_TRANSITION_H
#define JLINUX_TRANSITION_H
#include <stdbool.h>
struct transition { double from, to, start, duration; };
double transition_value(const struct transition *t, double now);
bool transition_active(const struct transition *t, double now);
void transition_target(struct transition *t, double target, double now, double duration);
#endif
