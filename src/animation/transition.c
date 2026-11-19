#include "transition.h"
bool transition_active(const struct transition *t, double now) {
    return t->from != t->to && t->duration > 0 && now < t->start + t->duration;
}
double transition_value(const struct transition *t, double now) {
    if (!transition_active(t, now)) { return t->to; }
    double p = (now - t->start) / t->duration;
    if (p < 0) { p = 0; }
    p = p * p * (3 - 2 * p);
    return t->from + (t->to - t->from) * p;
}
void transition_target(struct transition *t, double target, double now, double duration) {
    double value = transition_value(t, now);
    *t = (struct transition){value, target, now, duration};
}
