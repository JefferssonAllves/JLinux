#include "dock-transform.h"
void jlinux_dock_transform(cairo_matrix_t *matrix, double width, double height, double progress) {
    if (progress < 0) { progress = 0; } if (progress > 1) { progress = 1; }
    double scale = .18 + .82 * progress;
    cairo_matrix_init_translate(matrix, width / 2, height / 2);
    cairo_matrix_scale(matrix, scale, scale);
    cairo_matrix_translate(matrix, -width / 2, -height / 2);
}
