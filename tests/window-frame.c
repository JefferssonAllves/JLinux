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
    struct jlinux_glass effects = {.blur = true, .tint = true, .reflection = true, .shadow = true};
    CHECK(!jlinux_window_frame_update(&frame, 0, 400, "inválido", effects, false));
    CHECK(!jlinux_window_frame_update(&frame, 100, 8193, "inválido", effects, false));
    for (int i = 0; i < 12; i++) {
        CHECK(jlinux_window_frame_update(&frame, 700 + i, 400 + i, "JLinux Terminal", effects, i % 2 == 0));
        size_t pixels = 0;
        for (int j = 0; j < 4; j++) {
            if(!frame.parts[j]->buffer){CHECK(i%2==0&&j>0);continue;}
            pixels += (size_t)frame.parts[j]->buffer->width * frame.parts[j]->buffer->height;
        }
        CHECK(pixels < 40000);
        jlinux_window_frame_focus(&frame, false); CHECK(frame.parts[0]->opacity < 1);
        jlinux_window_frame_focus(&frame, true); CHECK(frame.parts[0]->opacity == 1);
    }
    void *pixels; uint32_t format; size_t stride;
    CHECK(wlr_buffer_begin_data_ptr_access(frame.parts[0]->buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
        &pixels, &format, &stride));
    uint32_t lit = *(uint32_t *)((unsigned char *)pixels + stride + 195 * 4);
    wlr_buffer_end_data_ptr_access(frame.parts[0]->buffer);
    CHECK(wlr_buffer_begin_data_ptr_access(frame.parts[3]->buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
        &pixels, &format, &stride));
    uint32_t left = *(uint32_t *)((unsigned char *)pixels + stride + 80 * 4);
    uint32_t middle = *(uint32_t *)((unsigned char *)pixels + stride + 360 * 4);
    uint32_t right = *(uint32_t *)((unsigned char *)pixels + stride + 640 * 4);
    uint32_t edge = *(uint32_t *)((unsigned char *)pixels + 2 * stride + 80 * 4);
    CHECK((edge & 255) > (left & 255));
    uint32_t pink = *(uint32_t *)((unsigned char *)pixels + 2 * stride + 230 * 4);
    CHECK(((pink >> 16) & 255) * 100 > (pink & 255) * 85);
    CHECK(((pink >> 16) & 255) > ((pink >> 8) & 255) * 2);
    /* Continuous light: no former bottom hotspot; hue still changes along the rim. */
    for (int x = 81; x < 640; x++) {
        uint32_t previous = *(uint32_t *)((unsigned char *)pixels + stride + (x - 1) * 4);
        uint32_t current = *(uint32_t *)((unsigned char *)pixels + stride + x * 4);
        int delta = (int)(current >> 24) - (int)(previous >> 24);
        CHECK(delta >= -2 && delta <= 2);
    }
    CHECK(left != middle && middle != right);
    CHECK((left & 255) > ((left >> 16) & 255));
    wlr_buffer_end_data_ptr_access(frame.parts[3]->buffer);

    effects.reflection = false;
    CHECK(jlinux_window_frame_update(&frame, 711, 411, "JLinux Terminal", effects, false));
    CHECK(wlr_buffer_begin_data_ptr_access(frame.parts[0]->buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
        &pixels, &format, &stride));
    uint32_t plain = *(uint32_t *)((unsigned char *)pixels + stride + 195 * 4);
    uint32_t header_center = *(uint32_t *)((unsigned char *)pixels + 18 * stride + 360 * 4);
    CHECK((header_center >> 24) >= 160 && (header_center >> 24) <= 220);
    CHECK(((header_center >> 16) & 255) <= 8 && ((header_center >> 8) & 255) <= 8 && (header_center & 255) <= 8);
    wlr_buffer_end_data_ptr_access(frame.parts[0]->buffer);
    CHECK((lit >> 24) > (plain >> 24));
    CHECK(wlr_buffer_begin_data_ptr_access(frame.parts[3]->buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
        &pixels, &format, &stride));
    uint32_t shaded = *(uint32_t *)((unsigned char *)pixels + stride + 280 * 4);
    wlr_buffer_end_data_ptr_access(frame.parts[3]->buffer);
    effects.shadow = false;
    CHECK(jlinux_window_frame_update(&frame, 711, 411, "JLinux Terminal", effects, false));
    CHECK(wlr_buffer_begin_data_ptr_access(frame.parts[3]->buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
        &pixels, &format, &stride));
    uint32_t unshaded = *(uint32_t *)((unsigned char *)pixels + stride + 280 * 4);
    wlr_buffer_end_data_ptr_access(frame.parts[3]->buffer);
    CHECK((shaded >> 24) > (unshaded >> 24));
    effects.tint = false;
    CHECK(jlinux_window_frame_update(&frame, 711, 411, "JLinux Terminal", effects, false));
    CHECK(wlr_buffer_begin_data_ptr_access(frame.parts[0]->buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
        &pixels, &format, &stride));
    CHECK(*(uint32_t *)((unsigned char *)pixels + 18 * stride + 360 * 4) == 0);
    wlr_buffer_end_data_ptr_access(frame.parts[0]->buffer);
    effects.tint = true; effects.shadow = true;

    effects.reflection = true;
    CHECK(jlinux_window_frame_update(&frame, 711, 411, "JLinux Terminal", effects, false));
    if (argc == 3 && !strcmp(argv[1], "--preview")) {
        cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 800, 520);
        cairo_t *cr = cairo_create(image); ui_wallpaper(cr, 800, 520);
        /* A component preview, not an application screenshot. */
        cairo_set_source_rgb(cr, .13, .17, .25); cairo_rectangle(cr, 44, 80, 711, 411); cairo_fill(cr);
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
    CHECK(jlinux_window_frame_update(&frame,711,411,"Maximized",effects,true));
    CHECK(frame.parts[0]->buffer->width==711&&frame.parts[0]->node.x==0);
    for(int i=1;i<4;i++){CHECK(!frame.parts[i]->node.enabled&&!frame.parts[i]->buffer);}
    CHECK(wlr_buffer_begin_data_ptr_access(frame.parts[0]->buffer,WLR_BUFFER_DATA_PTR_ACCESS_READ,&pixels,&format,&stride));
    uint32_t corner=*(uint32_t *)pixels;
    uint32_t center=*(uint32_t *)((unsigned char *)pixels+350*4);
    CHECK(corner==center&&(corner>>24)>100);
    CHECK(center==*(uint32_t *)((unsigned char *)pixels+(JLINUX_FRAME_HEADER-1)*stride+350*4));
    wlr_buffer_end_data_ptr_access(frame.parts[0]->buffer);
    CHECK(jlinux_window_frame_update(&frame,711,411,"Restored",effects,false));
    CHECK(frame.parts[0]->buffer->width==717&&frame.parts[0]->node.x==-3);
    for(int i=1;i<4;i++){CHECK(frame.parts[i]->node.enabled&&frame.parts[i]->buffer);}
    wlr_scene_node_destroy(&scene->tree.node); ui_finish(); return 0;
}
