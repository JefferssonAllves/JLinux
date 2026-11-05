#include "../src/renderer/window-frame.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wlr/types/wlr_buffer.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "Falha: %s\n", #test); return 1; } } while (0)
int main(int argc, char **argv) {
    struct wlr_scene *scene = wlr_scene_create(); CHECK(scene);
    struct jlinux_window_frame frame = {0};
    CHECK(jlinux_window_frame_create(&frame, &scene->tree));
    struct jlinux_glass effects = {.blur = true, .tint = true, .reflection = true};
    CHECK(!jlinux_window_frame_update(&frame, 0, 400, "inválido", effects));
    CHECK(!jlinux_window_frame_update(&frame, 100, 8193, "inválido", effects));
    for (int i = 0; i < 12; i++) {
        CHECK(jlinux_window_frame_update(&frame, 700 + i, 400 + i, "JLinux Terminal", effects));
        size_t pixels = 0;
        for (int j = 0; j < 4; j++) { pixels += (size_t)frame.parts[j]->buffer->width * frame.parts[j]->buffer->height; }
        CHECK(pixels < 40000);
        jlinux_window_frame_focus(&frame, false); CHECK(frame.parts[0]->opacity < 1);
        jlinux_window_frame_focus(&frame, true); CHECK(frame.parts[0]->opacity == 1);
    }
    if (argc == 3 && !strcmp(argv[1], "--preview")) {
        cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 800, 520);
        cairo_t *cr = cairo_create(image); ui_wallpaper(cr, 800, 520);
        /* A component preview, not an application screenshot. */
        cairo_set_source_rgb(cr, .025, .035, .065); cairo_rectangle(cr, 44, 80, 711, 411); cairo_fill(cr);
        ui_text(cr, "Prévia da moldura nativa", 64, 102, 16, false, 0xa9b7d0);
        for (int i = 0; i < 4; i++) {
            struct wlr_buffer *buffer = frame.parts[i]->buffer;
            void *data; uint32_t format; size_t stride;
            CHECK(wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride));
            cairo_surface_t *part = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32,
                buffer->width, buffer->height, (int)stride);
            cairo_set_source_surface(cr, part, 44 + frame.parts[i]->node.x, 80 + frame.parts[i]->node.y);
            cairo_paint(cr); cairo_surface_destroy(part); wlr_buffer_end_data_ptr_access(buffer);
        }
        CHECK(cairo_surface_write_to_png(image, argv[2]) == CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr); cairo_surface_destroy(image);
    }
    wlr_scene_node_destroy(&scene->tree.node); ui_finish(); return 0;
}
