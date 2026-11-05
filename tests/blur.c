#include "../src/renderer/blur.h"
#include <stdint.h>
#include <stdio.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "Falha: %s\n", #test); return 1; } } while (0)
int main(void) {
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 9, 9);
    uint32_t *pixels = (uint32_t *)cairo_image_surface_get_data(image);
    for (int i = 0; i < 81; i++) { pixels[i] = 0xff000000; }
    pixels[40] = 0xffffffff; cairo_surface_mark_dirty(image);
    CHECK(!jlinux_blur(image, -1)); CHECK(!jlinux_blur(image, 33));
    CHECK(jlinux_blur(image, 0)); CHECK(pixels[40] == 0xffffffff);
    CHECK(jlinux_blur(image, 1));
    CHECK(pixels[40] == pixels[39]); CHECK(pixels[40] == pixels[31]);
    CHECK((pixels[40] & 255) > 0 && (pixels[40] & 255) < 255);
    CHECK(pixels[0] == 0xff000000);
    for (int i = 0; i < 81; i++) { CHECK((pixels[i] >> 24) == 255); pixels[i] = 0xff426081; }
    cairo_surface_mark_dirty(image); CHECK(jlinux_blur(image, 5));
    for (int i = 0; i < 81; i++) { CHECK(pixels[i] == 0xff426081); }
    cairo_surface_destroy(image);
    image = cairo_image_surface_create(CAIRO_FORMAT_A8, 2, 2);
    CHECK(!jlinux_blur(image, 1)); cairo_surface_destroy(image);
    return 0;
}
