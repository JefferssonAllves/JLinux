#define _POSIX_C_SOURCE 200809L
#include <linux/input-event-codes.h>
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
    struct globals *globals = data;
    if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
        globals->manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        globals->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
int main(int argc, char **argv) {
    if (argc != 3 && argc != 5) { return 2; }
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) { return 1; }
    struct globals globals = {0};
    struct wl_registry *registry = wl_display_get_registry(display);
    const struct wl_registry_listener listener = {.global = global, .global_remove = removed};
    wl_registry_add_listener(registry, &listener, &globals);
    if (wl_display_roundtrip(display) < 0 || !globals.manager || !globals.seat) { return 1; }
    struct zwlr_virtual_pointer_v1 *pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(globals.manager, globals.seat);
    if (wl_display_roundtrip(display) < 0) { return 1; }
    struct timespec pause = {.tv_nsec = 150000000};
    nanosleep(&pause, NULL);
    zwlr_virtual_pointer_v1_motion_absolute(pointer, 1, (uint32_t)atoi(argv[1]), (uint32_t)atoi(argv[2]), 1280, 720);
    zwlr_virtual_pointer_v1_frame(pointer);
    if (wl_display_roundtrip(display) < 0) { return 1; }
    zwlr_virtual_pointer_v1_button(pointer, 2, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(pointer);
    if (argc == 5) {
        zwlr_virtual_pointer_v1_motion_absolute(pointer, 3, (uint32_t)atoi(argv[3]), (uint32_t)atoi(argv[4]), 1280, 720);
        zwlr_virtual_pointer_v1_frame(pointer);
        if (wl_display_roundtrip(display) < 0) { return 1; }
    }
    zwlr_virtual_pointer_v1_button(pointer, 3, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(pointer);
    if (wl_display_roundtrip(display) < 0) { return 1; }
    zwlr_virtual_pointer_v1_destroy(pointer);
    zwlr_virtual_pointer_manager_v1_destroy(globals.manager);
    wl_seat_destroy(globals.seat);
    wl_registry_destroy(registry);
    wl_display_flush(display);
    wl_display_disconnect(display);
    return 0;
}
