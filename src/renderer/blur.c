#include "blur.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
bool jlinux_blur(cairo_surface_t *image, int radius) {
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS ||
        cairo_surface_get_type(image) != CAIRO_SURFACE_TYPE_IMAGE ||
        cairo_image_surface_get_format(image) != CAIRO_FORMAT_ARGB32 || radius < 0 || radius > 32) { return false; }
    if (!radius) { return true; }
    int width = cairo_image_surface_get_width(image), height = cairo_image_surface_get_height(image);
    int stride = cairo_image_surface_get_stride(image);
    if (width < 1 || height < 1 || height > 8192 || width > 8192) { return false; }
    size_t size = (size_t)stride * height;
    uint8_t *scratch = malloc(size);
    if (!scratch) { return false; }
    cairo_surface_flush(image);
    uint8_t *pixels = cairo_image_surface_get_data(image);
    for (int pass = 0; pass < 2; pass++) {
        int outer = pass ? width : height, inner = pass ? height : width;
        for (int line = 0; line < outer; line++) {
            for (int pos = 0; pos < inner; pos++) {
                unsigned sums[4] = {0}; unsigned count = 0;
                int first = pos - radius > 0 ? pos - radius : 0;
                int last = pos + radius < inner ? pos + radius : inner - 1;
                for (int sample = first; sample <= last; sample++) {
                    size_t offset = pass ? (size_t)sample * stride + line * 4 : (size_t)line * stride + sample * 4;
                    for (int c = 0; c < 4; c++) { sums[c] += pixels[offset + c]; }
                    count++;
                }
                size_t offset = pass ? (size_t)pos * stride + line * 4 : (size_t)line * stride + pos * 4;
                for (int c = 0; c < 4; c++) { scratch[offset + c] = (uint8_t)(sums[c] / count); }
            }
        }
        memcpy(pixels, scratch, size);
    }
    free(scratch); cairo_surface_mark_dirty(image); return true;
}
