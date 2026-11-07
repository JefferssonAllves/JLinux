#include "window.h"
#include <math.h>
static void rounded(cairo_t *cr, struct ui_rect r, double radius) {
    const double pi=3.14159265358979323846;
    cairo_new_sub_path(cr);
    cairo_arc(cr,r.x+r.width-radius,r.y+radius,radius,-pi/2,0);
    cairo_arc(cr,r.x+r.width-radius,r.y+r.height-radius,radius,0,pi/2);
    cairo_arc(cr,r.x+radius,r.y+r.height-radius,radius,pi/2,pi);
    cairo_arc(cr,r.x+radius,r.y+radius,radius,pi,pi*1.5);cairo_close_path(cr);
}
void window_aura_paint(cairo_t *cr, struct ui_rect window, bool shadow, bool tint) {
    for(int distance=WINDOW_AURA_PAD;distance>0;distance--){
        double strength=pow(1-distance/(double)(WINDOW_AURA_PAD+1),2);
        struct ui_rect r={window.x-distance,window.y-distance+(shadow?4:0),window.width+2*distance,window.height+2*distance};
        rounded(cr,r,6+distance);
        if(shadow){cairo_set_source_rgba(cr,.003,.005,.018,strength*.19);}
        else{
            cairo_pattern_t *p=cairo_pattern_create_linear(window.x,window.y,window.x+window.width,window.y+window.height);
            cairo_pattern_add_color_stop_rgba(p,0,tint?.14:.6,tint?.47:.6,tint?1:.6,strength*.20);
            cairo_pattern_add_color_stop_rgba(p,1,tint?.73:.6,tint?.26:.6,tint?.96:.6,strength*.20);
            cairo_set_source(cr,p);cairo_pattern_destroy(p);
        }
        cairo_set_line_width(cr,2);cairo_stroke(cr);
    }
}
