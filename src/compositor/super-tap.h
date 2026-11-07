#ifndef JLINUX_SUPER_TAP_H
#define JLINUX_SUPER_TAP_H
#include <stdbool.h>
#include <stdint.h>
struct super_tap { uint32_t pressed, released; bool down, first; };
void super_tap_press(struct super_tap *tap,uint32_t now);
bool super_tap_release(struct super_tap *tap,uint32_t now);
void super_tap_cancel(struct super_tap *tap);
#endif
