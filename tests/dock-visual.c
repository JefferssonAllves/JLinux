#include "../src/renderer/dock-transform.h"
#include "../src/effects/dock.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Falha: %s\n",#x); return 1; } } while(0)
int main(void) {
    double previous = 0;
    for (int i = 0; i <= 20; i++) {
        cairo_matrix_t matrix; jlinux_dock_transform(&matrix, 1280, 720, i / 20.);
        if (i == 0) { CHECK(matrix.xx < .25); }
        if (i == 20) { CHECK(matrix.xx == 1); }
        double x = 640, y = 360;
        cairo_matrix_transform_point(&matrix, &x, &y);
        CHECK(fabs(x - 640) < 1e-8 && fabs(y - 360) < 1e-8);
        CHECK(matrix.xx >= previous && matrix.xx <= 1); previous = matrix.xx;
        x = 800; y = 600; cairo_matrix_transform_point(&matrix, &x, &y);
        CHECK(cairo_matrix_invert(&matrix) == CAIRO_STATUS_SUCCESS);
        cairo_matrix_transform_point(&matrix, &x, &y);
        CHECK(fabs(x - 800) < 1e-8 && fabs(y - 600) < 1e-8);
    }
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t *cr = cairo_create(image);
    jlinux_dock_halo(cr, 32, 32, 28, 0); cairo_surface_flush(image);
    uint32_t *pixels = (uint32_t *)cairo_image_surface_get_data(image);
    CHECK(pixels[32 * 64 + 32] == 0);
    jlinux_dock_halo(cr, 32, 32, 28, 1); cairo_surface_flush(image);
    CHECK(pixels[32 * 64 + 32] != 0 && pixels[0] == 0);
    CHECK(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
    cairo_destroy(cr); cairo_surface_destroy(image); return 0;
}
