#include "dock.h"
void jlinux_dock_halo(cairo_t *cr, double x, double y, double radius, double strength) {
    if (strength <= 0 || radius <= 0) { return; }
    if (strength > 1) { strength = 1; }
    cairo_save(cr);
    cairo_pattern_t *glow = cairo_pattern_create_radial(x, y, radius * .2, x, y, radius);
    cairo_pattern_add_color_stop_rgba(glow, 0, .38, .67, 1, .24 * strength);
    cairo_pattern_add_color_stop_rgba(glow, .65, .65, .40, 1, .12 * strength);
    cairo_pattern_add_color_stop_rgba(glow, 1, .4, .4, 1, 0);
    cairo_set_source(cr, glow); cairo_arc(cr, x, y, radius, 0, 6.283185307179586); cairo_fill(cr);
    cairo_pattern_destroy(glow); cairo_restore(cr);
}
static void material_stop(cairo_pattern_t *pattern,double offset,double r,double g,double b,double alpha,bool tint) {
    if(!tint){r=g=b=(r+g+b)/3;}
    cairo_pattern_add_color_stop_rgba(pattern,offset,r,g,b,alpha);
}
static void satin_reflection(cairo_t *cr,double focus,struct jlinux_glass effects) {
    if(!effects.reflection){return;}
    double x1,y1,x2,y2;cairo_path_extents(cr,&x1,&y1,&x2,&y2);
    cairo_save(cr);cairo_clip_preserve(cr);
    cairo_pattern_t *sheen=cairo_pattern_create_radial(x1+(x2-x1)*.26,y1+(y2-y1)*.12,0,x1+(x2-x1)*.26,y1+(y2-y1)*.12,(x2-x1+y2-y1)*.48);
    material_stop(sheen,0,.38+.14*focus,.46+.02*focus,.67+.05*focus,.18+.72*focus,effects.tint);
    material_stop(sheen,.38,.32+.12*focus,.40+.02*focus,.62+.06*focus,.11+.44*focus,effects.tint);
    material_stop(sheen,1,.20,.24,.42,0,effects.tint);
    cairo_set_source(cr,sheen);cairo_paint(cr);cairo_pattern_destroy(sheen);cairo_restore(cr);
}
void jlinux_dock_material(cairo_t *cr,double outer_x,double outer_y,double inner_x,double inner_y,double focus,struct jlinux_glass effects) {
    /* Graphite, blue-black and violet stops follow the generated reference. */
    cairo_pattern_t *base=cairo_pattern_create_linear(outer_x,outer_y,inner_x,inner_y);
    material_stop(base,0,.17+.025*focus,.21+.025*focus,.31+.04*focus,.99,effects.tint);
    material_stop(base,.26,.095,.12,.19,.99,effects.tint);
    material_stop(base,.50,.045,.055,.09,.99,effects.tint);
    material_stop(base,.74,.11,.06,.17,.99,effects.tint);
    material_stop(base,1,.19,.105,.28,.99,effects.tint);
    cairo_set_source(cr,base);cairo_fill_preserve(cr);cairo_pattern_destroy(base);
    satin_reflection(cr,focus,effects);
}
void jlinux_dock_center_material(cairo_t *cr,struct jlinux_glass effects) {
    double x1,y1,x2,y2;cairo_path_extents(cr,&x1,&y1,&x2,&y2);
    if(x2<=x1 || y2<=y1){return;}
    cairo_pattern_t *base=cairo_pattern_create_radial(x1+(x2-x1)*.25,y1+(y2-y1)*.22,0,(x1+x2)/2,(y1+y2)/2,(x2-x1)*.60);
    material_stop(base,0,.23,.25,.38,.99,effects.tint);
    material_stop(base,.45,.10,.12,.19,.99,effects.tint);
    material_stop(base,1,.035,.04,.075,.99,effects.tint);
    cairo_set_source(cr,base);cairo_fill_preserve(cr);cairo_pattern_destroy(base);
    satin_reflection(cr,0,effects);
}

void jlinux_dock_edge_glow(cairo_t *cr,double strength) {
    if(strength<=0){return;}
    double x1,y1,x2,y2;cairo_path_extents(cr,&x1,&y1,&x2,&y2);
    for(int i=6;i>=1;i--){
        cairo_pattern_t *light=cairo_pattern_create_linear(x1,y1,x2,y2);
        double alpha=.32*strength/(i*i);
        cairo_pattern_add_color_stop_rgba(light,0,.08,.86,1,alpha);
        cairo_pattern_add_color_stop_rgba(light,.5,.40,.48,1,alpha);
        cairo_pattern_add_color_stop_rgba(light,1,.85,.20,1,alpha);
        cairo_set_source(cr,light);cairo_set_line_width(cr,1.8+2*i);cairo_stroke_preserve(cr);cairo_pattern_destroy(light);
    }
    cairo_set_source_rgba(cr,.75,.22,1,.55*strength);
    cairo_set_line_width(cr,4.5);cairo_stroke_preserve(cr);
}
