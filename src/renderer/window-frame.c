#include "window-frame.h"
#include <drm_fourcc.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
struct frame_buffer {
    struct wlr_buffer base;
    cairo_surface_t *image;
};
static void destroy(struct wlr_buffer *base) {
    struct frame_buffer *buffer = wl_container_of(base, buffer, base);
    cairo_surface_destroy(buffer->image); free(buffer);
}
static bool access_pixels(struct wlr_buffer *base, uint32_t flags, void **data,
        uint32_t *format, size_t *stride) {
    struct frame_buffer *buffer = wl_container_of(base, buffer, base);
    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) { return false; }
    *data = cairo_image_surface_get_data(buffer->image);
    *format = DRM_FORMAT_ARGB8888;
    *stride = (size_t)cairo_image_surface_get_stride(buffer->image);
    return true;
}
static void end_access(struct wlr_buffer *base) { (void)base; }
static const struct wlr_buffer_impl impl = {
    .destroy = destroy, .begin_data_ptr_access = access_pixels, .end_data_ptr_access = end_access,
};
bool jlinux_window_frame_create(struct jlinux_window_frame *frame, struct wlr_scene_tree *parent) {
    for (int i = 0; i < 4; i++) {
        frame->parts[i] = wlr_scene_buffer_create(parent, NULL);
        if (!frame->parts[i]) { return false; }
    }
    return true;
}
bool jlinux_window_frame_update(struct jlinux_window_frame *frame, int width, int height,
        const char *title, struct jlinux_glass effects) {
    /* shortcut: live transparency only; backdrop blur awaits the compositor GPU passes. */
    if (width < 1 || height < 1 || width > 8192 || height > 8192) { return false; }
    const int border = JLINUX_FRAME_BORDER, header = JLINUX_FRAME_HEADER;
    int full_width = width + 2 * border, full_height = height + header + border;
    /* Four narrow strips avoid allocating a full-window transparent image. */
    const int boxes[4][4] = {{0, 0, full_width, header}, {0, header, border, height},
        {full_width - border, header, border, height}, {0, header + height, full_width, border}};
    struct frame_buffer *buffers[4] = {0};
    for (int i = 0; i < 4; i++) {
        buffers[i] = calloc(1, sizeof(*buffers[i]));
        if (!buffers[i]) { goto fail; }
        buffers[i]->image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, boxes[i][2], boxes[i][3]);
        cairo_t *cr = cairo_create(buffers[i]->image);
        cairo_translate(cr, -boxes[i][0], -boxes[i][1]);
        /* Extend the header below its strip so its lower edge stays square. */
        int material_height = i == 0 ? header + JLINUX_FRAME_RADIUS : full_height;
        jlinux_glass_paint(cr, (struct ui_rect){.5, .5, full_width - 1, material_height - 1},
            JLINUX_FRAME_RADIUS, NULL, 0, 0, effects);
        if (i == 0) {
            cairo_save(cr); cairo_rectangle(cr, 16, 3, full_width - 70, header - 6); cairo_clip(cr);
            ui_text(cr, title ? title : "JLinux", 16, 8, 14, true, 0xedf0ff); cairo_restore(cr);
            cairo_set_source_rgba(cr, .77, .82, 1, .16); cairo_set_line_width(cr, 1);
            cairo_move_to(cr, 8, header - .5); cairo_line_to(cr, full_width - 8, header - .5); cairo_stroke(cr);
            double x = full_width - 24;
            cairo_set_source_rgba(cr, .94, .95, 1, .92); cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, x - 4, 14); cairo_line_to(cr, x + 4, 22);
            cairo_move_to(cr, x + 4, 14); cairo_line_to(cr, x - 4, 22); cairo_stroke(cr);
        }
        bool valid = cairo_status(cr) == CAIRO_STATUS_SUCCESS &&
            cairo_surface_status(buffers[i]->image) == CAIRO_STATUS_SUCCESS;
        cairo_destroy(cr);
        if (!valid) { goto fail; }
        cairo_surface_flush(buffers[i]->image);
    }
    for (int i = 0; i < 4; i++) {
        wlr_buffer_init(&buffers[i]->base, &impl, boxes[i][2], boxes[i][3]);
        wlr_scene_buffer_set_buffer(frame->parts[i], &buffers[i]->base);
        wlr_scene_node_set_position(&frame->parts[i]->node, boxes[i][0] - border, boxes[i][1] - header);
        wlr_buffer_drop(&buffers[i]->base);
    }
    return true;
fail:
    for (int i = 0; i < 4; i++) {
        if (buffers[i]) { if (buffers[i]->image) { cairo_surface_destroy(buffers[i]->image); } free(buffers[i]); }
    }
    return false;
}
void jlinux_window_frame_focus(struct jlinux_window_frame *frame, bool focused) {
    for (int i = 0; i < 4; i++) { wlr_scene_buffer_set_opacity(frame->parts[i], focused ? 1 : .72); }
}
