#define _POSIX_C_SOURCE 200809L
#include <linux/input-event-codes.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include "virtual-pointer-client.h"
struct globals { struct zwlr_virtual_pointer_manager_v1 *manager; struct wl_seat *seat; };
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)version;
    struct globals *g = data;
    if (!strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name))
        g->manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
    else if (!strcmp(interface, wl_seat_interface.name))
        g->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
static void interrupted(int signal) { (void)signal; }
static uint32_t timestamp(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000);
}
int main(void) {
    struct sigaction action = {.sa_handler = interrupted};
    sigaction(SIGTERM, &action, NULL);
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) return 1;
    struct globals g = {0};
    struct wl_registry *registry = wl_display_get_registry(display);
    const struct wl_registry_listener listener = {.global = global, .global_remove = removed};
    wl_registry_add_listener(registry, &listener, &g);
    int result = 1;
    if (wl_display_roundtrip(display) < 0 || !g.manager || !g.seat) goto cleanup;
    struct zwlr_virtual_pointer_v1 *pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(g.manager, g.seat);
    if (wl_display_roundtrip(display) < 0) { zwlr_virtual_pointer_v1_destroy(pointer); goto cleanup; }
    puts("ready"); fflush(stdout);
    char line[128], tail;
    double dx, dy;
    int pressed = 0, value;
    result = 0;
    while (fgets(line, sizeof(line), stdin)) {
        if (sscanf(line, "move %lf %lf %c", &dx, &dy, &tail) == 2 && isfinite(dx) && isfinite(dy) && fabs(dx) <= 4096 && fabs(dy) <= 4096) {
            zwlr_virtual_pointer_v1_motion(pointer, timestamp(), wl_fixed_from_double(dx), wl_fixed_from_double(dy));
        } else if (sscanf(line, "button %d %c", &value, &tail) == 1 && (value == 0 || value == 1)) {
            pressed = value;
            zwlr_virtual_pointer_v1_button(pointer, timestamp(), BTN_LEFT, pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
        } else { result = 2; break; }
        zwlr_virtual_pointer_v1_frame(pointer);
        if (wl_display_roundtrip(display) < 0) { result = 1; break; }
        puts("ok"); fflush(stdout);
    }
    if (pressed) {
        zwlr_virtual_pointer_v1_button(pointer, timestamp(), BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
        zwlr_virtual_pointer_v1_frame(pointer);
        wl_display_roundtrip(display);
    }
    zwlr_virtual_pointer_v1_destroy(pointer);
cleanup:
    if (g.manager) zwlr_virtual_pointer_manager_v1_destroy(g.manager);
    if (g.seat) wl_seat_destroy(g.seat);
    wl_registry_destroy(registry);
    wl_display_flush(display);
    wl_display_disconnect(display);
    return result;
}
