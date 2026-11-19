#include "../src/effects/glass.h"
#include <stdint.h>
#include <stdio.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "Falha: %s\n", #test); return 1; } } while (0)
static unsigned alpha(cairo_surface_t *image, int x, int y) {
    cairo_surface_flush(image);
    uint32_t *row = (uint32_t *)(cairo_image_surface_get_data(image) + y * cairo_image_surface_get_stride(image));
    return row[x] >> 24;
}
int main(void) {
    cairo_surface_t *backdrop = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 80, 13);
    cairo_t *background = cairo_create(backdrop);
    cairo_set_source_rgb(background, .08, .12, .22); cairo_paint(background); cairo_destroy(background);
    for (int tint = 0; tint < 2; tint++) {
        for (int reflection = 0; reflection < 2; reflection++) {
            cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 320, 52);
            cairo_t *cr = cairo_create(image);
            jlinux_glass_panel_paint(cr, 320, 36, 16, backdrop,
                (struct jlinux_glass){.tint = tint, .reflection = reflection});
            CHECK(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
            CHECK(alpha(image, 0, 0) == 255 && alpha(image, 319, 0) == 255);
            CHECK(alpha(image, 0, 30) == 255 && alpha(image, 319, 30) == 255);
            for (int x = 0; x < 320; x++) {
                unsigned previous = alpha(image, x, 35);
                for (int y = 36; y < 52; y++) {
                    unsigned current = alpha(image, x, y);
                    CHECK(current < previous && previous - current <= 17);
                    previous = current;
                }
                CHECK(previous <= 8);
            }
            cairo_destroy(cr); cairo_surface_destroy(image);
        }
    }
    cairo_surface_destroy(backdrop);
    return 0;
}
