#include "futurista.h"
#include "battery.h"
#include "apps.h"
#include "../renderer/blur.h"
#include "../effects/dock.h"
#include <gio/gio.h>
#include <glob.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static const double pi = 3.14159265358979323846;
struct png_reader {
    const unsigned char *bytes;
    size_t left;
};
static cairo_status_t read_png(void *data, unsigned char *out, unsigned int length) {
    struct png_reader *reader = data;
    if (length > reader->left) {
        return CAIRO_STATUS_READ_ERROR;
    }
    memcpy(out, reader->bytes, length);
    reader->bytes += length;
    reader->left -= length;
    return CAIRO_STATUS_SUCCESS;
}
static cairo_surface_t *asset(const char *name) {
    GBytes *bytes = g_resources_lookup_data(name, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    if (!bytes) {
        return NULL;
    }
    struct png_reader reader;
    reader.bytes = g_bytes_get_data(bytes, &reader.left);
    cairo_surface_t *image = cairo_image_surface_create_from_png_stream(read_png, &reader);
    g_bytes_unref(bytes);
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(image);
        return NULL;
    }
    return image;
}
bool futurista_init(struct futurista *s, struct jlinux_glass glass) {
    *s = (struct futurista){.glass = glass, .wifi = -1, .battery = -1, .volume = -1, .brightness = -1};
    s->hover[s->selected] = 1;
    s->wallpaper = asset("/org/jlinux/futurista/wallpaper.png");
    s->prefs = shell_preferences_default();
    for (int i=0;i<10;i++) { s->icons[i] = dock_app_icon(i); }
    if (s->wallpaper && glass.blur) {
        s->blurred = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 418, 236);
        cairo_t *cr = cairo_create(s->blurred);
        futurista_wallpaper(s, cr, 418, 236);
        cairo_destroy(cr);
        if (!jlinux_blur(s->blurred, 7)) {
            return false;
        }
    }
    return s->wallpaper != NULL;
}
void futurista_finish(struct futurista *s) {
    if (s->blurred) {
        cairo_surface_destroy(s->blurred);
    }
    if (s->wallpaper) {
        cairo_surface_destroy(s->wallpaper);
    }
    for (int i=0;i<10;i++) { if (s->icons[i]) { cairo_surface_destroy(s->icons[i]); } }

}
static void roundrect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -pi / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, pi / 2);
    cairo_arc(cr, x + r, y + h - r, r, pi / 2, pi);
    cairo_arc(cr, x + r, y + r, r, pi, pi * 1.5);
    cairo_close_path(cr);
}
static void image(cairo_t *cr, cairo_surface_t *img, double x, double y, double size) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / cairo_image_surface_get_width(img), size / cairo_image_surface_get_height(img));
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}
static void text(cairo_t *cr, const char *label, double x, double y, int size, bool bold, unsigned color) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_new();
    pango_font_description_set_family(font, "Adwaita Sans, Sans");
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    pango_font_description_set_weight(font, bold ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, label, -1);
    cairo_set_source_rgb(cr, ((color >> 16) & 255) / 255., ((color >> 8) & 255) / 255., (color & 255) / 255.);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);
}
static void centered_text(cairo_t *cr,const char *label,double y,int size,bool bold,unsigned color) {
    PangoLayout *layout=pango_cairo_create_layout(cr);
    PangoFontDescription *font=pango_font_description_from_string("Adwaita Sans, Sans");
    pango_font_description_set_absolute_size(font,size*PANGO_SCALE);
    pango_font_description_set_weight(font,bold?PANGO_WEIGHT_SEMIBOLD:PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout,font);pango_layout_set_text(layout,label,-1);
    int width;pango_layout_get_pixel_size(layout,&width,NULL);
    text(cr,label,836-width/2.,y,size,bold,color);
    pango_font_description_free(font);g_object_unref(layout);
}
static void material(cairo_t *cr, double x, double y, double w, double h, double r, bool active,
                     struct jlinux_glass effects) {
    if (effects.shadow) {
        roundrect(cr, x + 1, y + 3, w, h, r);
        cairo_set_source_rgba(cr, .006, .004, .025, .28);
        cairo_fill(cr);
    }
    bool panel = w > 450 && h > 500;
    roundrect(cr, x, y, w, h, r);
    cairo_pattern_t *p = cairo_pattern_create_linear(x, y, x + w, y + h);
    if (active && effects.tint) {
        cairo_pattern_add_color_stop_rgba(p, 0, .53, .25, 1, .98);
        cairo_pattern_add_color_stop_rgba(p, .5, w > 200 ? .51 : .35, w > 200 ? .27 : .20,
                                          w > 200 ? .97 : .83, .97);
        cairo_pattern_add_color_stop_rgba(p, 1, .50, .29, .96, .98);
    } else {
        cairo_pattern_add_color_stop_rgba(p, 0, effects.tint ? (panel ? .08 : .17) : .18,
                                          effects.tint ? (panel ? .10 : .19) : .18,
                                          effects.tint ? (panel ? .24 : .35) : .18, .97);
        cairo_pattern_add_color_stop_rgba(p, .42, effects.tint ? (panel ? .055 : .11) : .12,
                                          effects.tint ? (panel ? .07 : .14) : .12,
                                          effects.tint ? (panel ? .18 : .28) : .12, .97);
        cairo_pattern_add_color_stop_rgba(p, 1, effects.tint ? (panel ? .15 : .20) : .18,
                                          effects.tint ? (panel ? .10 : .18) : .18,
                                          effects.tint ? (panel ? .30 : .37) : .18, .97);
    }
    cairo_set_source(cr, p);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(p);
    if (effects.reflection) {
        cairo_save(cr);
        cairo_clip_preserve(cr);
        cairo_pattern_t *light = cairo_pattern_create_linear(x, y, x, y + h);
        cairo_pattern_add_color_stop_rgba(light, 0, .85, .86, 1, active ? .30 : panel ? .10 : .12);
        cairo_pattern_add_color_stop_rgba(light, .15, .60, .62, 1, .035);
        cairo_pattern_add_color_stop_rgba(light, .7, .5, .4, .9, 0);
        cairo_pattern_add_color_stop_rgba(light, 1, .7, .3, 1, active ? .22 : .08);
        cairo_set_source(cr, light);
        cairo_paint(cr);
        cairo_pattern_destroy(light);
        cairo_restore(cr);
        p = cairo_pattern_create_linear(x, y, x, y + h);
        cairo_pattern_add_color_stop_rgba(p, 0, .70, .73, 1, active ? .95 : .42);
        cairo_pattern_add_color_stop_rgba(p, .4, .49, .43, .80, .16);
        cairo_pattern_add_color_stop_rgba(p, 1, .78, .35, 1, active || panel ? 1 : .50);
        if (panel || active) {
            cairo_set_source_rgba(cr, .69, .30, 1, .12);
            cairo_set_line_width(cr, panel ? 10 : 7);
            cairo_stroke_preserve(cr);
            cairo_set_source_rgba(cr, .77, .41, 1, .22);
            cairo_set_line_width(cr, 3);
            cairo_stroke_preserve(cr);
        }
        cairo_set_source(cr, p);
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);
        cairo_pattern_destroy(p);
    } else {
        cairo_new_path(cr);
    }
}
static void circle(cairo_t *cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * pi); }
static void line_icon(cairo_t *cr, int kind, double x, double y, double size, unsigned color) {
    if (kind == 0 || kind == 1 || kind == 10) {
        ui_icon(cr, kind == 0 ? UI_WIFI : kind == 1 ? UI_BLUETOOTH : UI_POWER, x, y, size, color);
        return;
    }
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, size / 32, size / 32);
    cairo_set_source_rgb(cr, ((color >> 16) & 255) / 255., ((color >> 8) & 255) / 255., (color & 255) / 255.);
    cairo_set_line_width(cr, 2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    switch (kind) {
    case 2:
        cairo_move_to(cr, 3, 14);
        cairo_line_to(cr, 13, 16);
        cairo_line_to(cr, 11, 4);
        cairo_line_to(cr, 14, 4);
        cairo_line_to(cr, 20, 16);
        cairo_line_to(cr, 29, 19);
        cairo_line_to(cr, 29, 22);
        cairo_line_to(cr, 19, 21);
        cairo_line_to(cr, 17, 29);
        cairo_line_to(cr, 14, 29);
        cairo_line_to(cr, 14, 21);
        cairo_line_to(cr, 3, 17);
        break;
    case 3:
        cairo_arc(cr, 17, 15, 12, .4, 4.4);
        cairo_curve_to(cr, 8, 19, 13, 26, 27, 20);
        break;
    case 4:
        cairo_move_to(cr, 6, 27);
        cairo_curve_to(cr, 5, 10, 12, 5, 27, 4);
        cairo_curve_to(cr, 28, 24, 19, 29, 8, 22);
        cairo_move_to(cr, 6, 28);
        cairo_line_to(cr, 23, 9);
        break;
    case 5:
        circle(cr, 16, 16, 6);
        for (int i = 0; i < 8; i++) {
            double a = i * pi / 4;
            cairo_move_to(cr, 16 + 11 * cos(a), 16 + 11 * sin(a));
            cairo_line_to(cr, 16 + 14 * cos(a), 16 + 14 * sin(a));
        }
        break;
    case 6:
        for (int i = 0; i < 4; i++) {
            cairo_save(cr);
            cairo_translate(cr, 16, 16);
            cairo_rotate(cr, i * pi / 2);
            cairo_move_to(cr, -12, -5);
            cairo_line_to(cr, -12, -12);
            cairo_line_to(cr, -5, -12);
            cairo_restore(cr);
        }
        break;
    case 7:
        cairo_rectangle(cr, 2, 4, 28, 20);
        cairo_move_to(cr, 16, 24);
        cairo_line_to(cr, 16, 29);
        cairo_move_to(cr, 9, 29);
        cairo_line_to(cr, 23, 29);
        break;
    case 8:
        cairo_move_to(cr, 15, 8);
        cairo_line_to(cr, 22, 8);
        cairo_line_to(cr, 29, 16);
        cairo_line_to(cr, 22, 24);
        cairo_line_to(cr, 15, 24);
        cairo_move_to(cr, 4, 6);
        cairo_line_to(cr, 9, 6);
        cairo_move_to(cr, 4, 6);
        cairo_line_to(cr, 4, 26);
        cairo_line_to(cr, 9, 26);
        break;
    case 9:
        cairo_arc(cr, 16, 16, 12, .2, 5.1);
        cairo_move_to(cr, 17, 2);
        cairo_line_to(cr, 23, 5);
        cairo_line_to(cr, 18, 10);
        break;
    case 11:
        cairo_move_to(cr, 3, 12);
        cairo_line_to(cr, 8, 12);
        cairo_line_to(cr, 15, 5);
        cairo_line_to(cr, 15, 27);
        cairo_line_to(cr, 8, 20);
        cairo_line_to(cr, 3, 20);
        cairo_close_path(cr);
        cairo_move_to(cr, 21, 8);
        cairo_curve_to(cr, 29, 10, 29, 22, 21, 25);
        cairo_move_to(cr, 20, 12);
        cairo_curve_to(cr, 23, 14, 23, 19, 20, 21);
        break;
    case 13:
        for (int i = 0; i < 3; i++) {
            double y = 6 + i * 10, knob = i == 1 ? 21 : 10;
            cairo_move_to(cr, 3, y);
            cairo_line_to(cr, knob - 3, y);
            cairo_move_to(cr, knob + 3, y);
            cairo_line_to(cr, 29, y);
            cairo_new_sub_path(cr);
            circle(cr, knob, y, 3);
        }
        break;
    case 12:
        circle(cr, 13, 13, 10);
        cairo_move_to(cr, 21, 21);
        cairo_line_to(cr, 29, 29);
        break;
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}
static void app_icon(struct futurista *s, cairo_t *cr, int index, double x, double y, double size) {
    if (s->icons[index]) { image(cr, s->icons[index], x, y, size); return; }
    /* Missing system icons are explicit initials, never another application's logo. */
    char initial[8] = {0}; g_unichar_to_utf8(g_utf8_get_char(dock_app_name(index)), initial);
    text(cr, initial, x+size*.3, y+size*.2, size*.6, true, 0xddeaff);
}

void futurista_wallpaper(struct futurista *s, cairo_t *cr, int width, int height) {
    double iw = cairo_image_surface_get_width(s->wallpaper),
           ih = cairo_image_surface_get_height(s->wallpaper);
    double scale = fmax(width / iw, height / ih);
    cairo_save(cr);
    cairo_translate(cr, (width - iw * scale) / 2, (height - ih * scale) / 2);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, s->wallpaper, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}
static void blurred_backdrop(struct futurista *s, cairo_t *cr, int width, int height) {
    if (!s->blurred) {
        return;
    }
    cairo_save(cr);
    cairo_clip(cr);
    cairo_scale(cr, width / (double)cairo_image_surface_get_width(s->blurred),
                height / (double)cairo_image_surface_get_height(s->blurred));
    cairo_set_source_surface(cr, s->blurred, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
}
static int media_width(int width) { return MIN(476, MAX(180,(width-360)/2-24)); }
int futurista_media_hit(double x,double y,int width) {
    double scale=FUTURISTA_PANEL_HEIGHT/56.;x/=scale;y/=scale;width=(int)(width/scale);
    if(width<800&&width>0){double scale=800./width;x*=scale;y*=scale;width=800;}
    int right=18+media_width(width);
    if(y<6||y>48||x<right-138||x>=right-6){return -1;}
    return (int)(x-(right-138))/44;
}
static void panel_card(cairo_t *cr,double x,double y,double w,double h,struct jlinux_glass glass) {
    roundrect(cr,x,y,w,h,10);
    cairo_pattern_t *p=cairo_pattern_create_linear(x,y,x,y+h);
    cairo_pattern_add_color_stop_rgba(p,0,glass.tint?.22:.16,.22,glass.tint?.34:.16,.16);
    cairo_pattern_add_color_stop_rgba(p,.5,.035,.045,.09,.18);
    cairo_pattern_add_color_stop_rgba(p,1,.08,.10,.18,glass.shadow?.26:.12);
    cairo_set_source(cr,p);cairo_fill_preserve(cr);cairo_pattern_destroy(p);
    cairo_set_source_rgba(cr,.72,.77,.94,glass.reflection?.22:.08);cairo_set_line_width(cr,.8);cairo_stroke(cr);
    if(glass.reflection){
        cairo_save(cr);roundrect(cr,x+1,y+1,w-2,h-2,9);cairo_clip(cr);
        cairo_pattern_t *sheen=cairo_pattern_create_linear(x,y,x,y+h*.55);
        cairo_pattern_add_color_stop_rgba(sheen,0,.80,.87,1,.12);
        cairo_pattern_add_color_stop_rgba(sheen,1,.80,.87,1,0);
        cairo_set_source(cr,sheen);cairo_paint(cr);cairo_pattern_destroy(sheen);cairo_restore(cr);
    }
}
static void media_symbol(cairo_t *cr,int action,bool playing,double x,double y,bool enabled) {
    cairo_save(cr);cairo_translate(cr,x,y);cairo_set_source_rgba(cr,.97,.94,1,enabled?1:.35);
    cairo_set_line_width(cr,2);cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);
    if(action==1&&playing){cairo_rectangle(cr,-6,-8,3,16);cairo_rectangle(cr,3,-8,3,16);cairo_fill(cr);}
    else if(action==1){cairo_move_to(cr,-6,-9);cairo_line_to(cr,9,0);cairo_line_to(cr,-6,9);cairo_close_path(cr);cairo_stroke(cr);}
    else {if(action==0){cairo_scale(cr,-1,1);}cairo_move_to(cr,-6,-7);cairo_line_to(cr,5,0);cairo_line_to(cr,-6,7);cairo_close_path(cr);cairo_fill(cr);cairo_move_to(cr,7,-8);cairo_line_to(cr,7,8);cairo_stroke(cr);}
    cairo_restore(cr);
}
void futurista_panel(struct futurista *s,cairo_t *cr,int width) {
    cairo_save(cr);
    double scale=FUTURISTA_PANEL_HEIGHT/56.;cairo_scale(cr,scale,scale);width=(int)(width/scale);
    if(width<800&&width>0){cairo_scale(cr,width/800.,width/800.);width=800;}
    cairo_pattern_t *p=cairo_pattern_create_linear(0,0,width,56);
    cairo_pattern_add_color_stop_rgb(p,0,.035,.035,.09);cairo_pattern_add_color_stop_rgb(p,1,.075,.045,.15);
    cairo_rectangle(cr,0,0,width,56);blurred_backdrop(s,cr,width,941/scale);
    cairo_rectangle(cr,0,0,width,56);cairo_set_source(cr,p);cairo_clip(cr);cairo_paint_with_alpha(cr,.52);cairo_reset_clip(cr);cairo_pattern_destroy(p);
    cairo_set_source_rgba(cr,.70,.75,.92,.10);cairo_set_line_width(cr,1);cairo_move_to(cr,0,55.5);cairo_line_to(cr,width,55.5);cairo_stroke(cr);
    int mw=media_width(width),right=18+mw;
    panel_card(cr,18,6,mw,42,s->glass);
    text(cr,"♫",32,6,30,true,0xb8b5df);
    struct media *m=s->media;
    cairo_save(cr);cairo_rectangle(cr,82,8,MAX(0,mw-222),38);cairo_clip(cr);
    text(cr,m&&m->player?(m->title[0]?m->title:"Mídia em reprodução"):"Nenhuma mídia",82,8,15,true,0xf4edff);
    text(cr,m&&m->player?(m->artist[0]?m->artist:(m->playing?"Reproduzindo":"Pausado")):"Abra um player",82,26,13,false,0xd5c2ee);cairo_restore(cr);
    for(int i=0;i<3;i++){bool enabled=m&&m->can_control&&m->player&&(i==0?m->can_previous:i==2?m->can_next:m->playing?m->can_pause:m->can_play);media_symbol(cr,i,m&&m->playing,right-116+i*44,27,enabled);}
    double cx=(width-350)/2.;panel_card(cr,cx,6,350,42,s->glass);
    /* Calendar outline and binding rings share the reference's purple accent. */
    cairo_set_source_rgb(cr,.69,.71,.83);roundrect(cr,cx+20,17,23,22,3);cairo_set_line_width(cr,2);cairo_stroke(cr);
    cairo_move_to(cr,cx+20,24);cairo_line_to(cr,cx+43,24);cairo_move_to(cr,cx+26,13);cairo_line_to(cr,cx+26,20);cairo_move_to(cr,cx+37,13);cairo_line_to(cr,cx+37,20);cairo_stroke(cr);
    for(int row=0;row<2;row++)for(int col=0;col<3;col++){cairo_rectangle(cr,cx+25+col*6,28+row*5,2,2);cairo_fill(cr);}
    time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
    static const char *days[]={"Dom","Seg","Ter","Qua","Qui","Sex","Sáb"};
    static const char *months[]={"Jan","Fev","Mar","Abr","Mai","Jun","Jul","Ago","Set","Out","Nov","Dez"};
    char date[64],clock[16];snprintf(date,sizeof(date),"%s, %02d %s",days[t.tm_wday],t.tm_mday,months[t.tm_mon]);snprintf(clock,sizeof(clock),"%02d:%02d",t.tm_hour,t.tm_min);
    text(cr,date,cx+64,14,20,true,0xf4edff);
    cairo_set_source_rgba(cr,.75,.79,.92,.20);cairo_move_to(cr,cx+224,13);cairo_line_to(cr,cx+224,41);cairo_stroke(cr);
    text(cr,clock,cx+246,11,26,true,0xfaf5ff);
    cairo_set_source_rgb(cr,.94,.94,1);roundrect(cr,width-181,18,29,16,3);cairo_set_line_width(cr,1.5);cairo_stroke(cr);cairo_rectangle(cr,width-150,23,3,6);cairo_fill(cr);
    if(s->battery>=0){cairo_rectangle(cr,width-178,21,23*s->battery/100.,10);cairo_fill(cr);}
    char battery[20];snprintf(battery,sizeof(battery),s->battery>=0?"%d%%":"—",s->battery);text(cr,battery,width-134,14,20,true,0xf4edff);
    line_icon(cr,13,width-46,16,24,0xf1eaff);
    cairo_restore(cr);
}
static void sector(cairo_t *cr, int index, double focus, struct jlinux_glass effects) {
    double a = (-108 + index * 36 + .6) * pi / 180, b = (-72 + index * 36 - .6) * pi / 180;
    double outer = 326 + 4 * focus, inner = 176 - 11 * focus;
    const double corner = 5;
    cairo_new_path(cr);
    cairo_arc(cr, 836, 469, outer, a + corner / outer, b - corner / outer);
    cairo_curve_to(cr, 836 + outer * cos(b), 469 + outer * sin(b), 836 + outer * cos(b), 469 + outer * sin(b),
                   836 + (outer - corner) * cos(b), 469 + (outer - corner) * sin(b));
    cairo_line_to(cr, 836 + (inner + corner) * cos(b), 469 + (inner + corner) * sin(b));
    cairo_curve_to(cr, 836 + inner * cos(b), 469 + inner * sin(b), 836 + inner * cos(b), 469 + inner * sin(b),
                   836 + inner * cos(b - corner / inner), 469 + inner * sin(b - corner / inner));
    cairo_arc_negative(cr, 836, 469, inner, b - corner / inner, a + corner / inner);
    cairo_curve_to(cr, 836 + inner * cos(a), 469 + inner * sin(a), 836 + inner * cos(a), 469 + inner * sin(a),
                   836 + (inner + corner) * cos(a), 469 + (inner + corner) * sin(a));
    cairo_line_to(cr, 836 + (outer - corner) * cos(a), 469 + (outer - corner) * sin(a));
    cairo_curve_to(cr, 836 + outer * cos(a), 469 + outer * sin(a), 836 + outer * cos(a), 469 + outer * sin(a),
                   836 + outer * cos(a + corner / outer), 469 + outer * sin(a + corner / outer));
    cairo_close_path(cr);
    double mid=(a+b)/2;
    jlinux_dock_material(cr,836+outer*cos(mid),469+outer*sin(mid),836+inner*cos(mid),469+inner*sin(mid),focus,effects);
    if (effects.reflection) {
        jlinux_dock_edge_glow(cr,focus);
        jlinux_glass_rim(cr, (struct ui_rect){506,139,660,660}, .75 + .25 * focus);
        cairo_new_path(cr);
    } else {
        cairo_new_path(cr);
    }
}
static void settings(struct futurista *s, cairo_t *cr) {
    struct ui_rect panel = {350,140,1020,650};
    jlinux_glass_window_tint(cr,panel,24);
    if(s->glass.shadow){jlinux_glass_window_shading(cr,panel,24);}
    if(s->glass.reflection){jlinux_glass_window_reflections(cr,panel,24,70);}
    text(cr,"Configurações",385,165,28,true,0xf2edff);
    text(cr,"×",1315,160,32,false,0xbfdfff);
    const char *categories[]={"Dock","Aparência","Sistema"};
    for(int i=0;i<3;i++){
        material(cr,380,270+i*92,215,66,14,i==s->settings_category,s->glass);
        text(cr,categories[i],405,290+i*92,21,true,0xebeeff);
    }
    text(cr,categories[s->settings_category],640,235,26,true,0xebedff);
    if(s->settings_category < 2){
        const char *dock[]={"Animação de abrir e fechar","Ampliar ícone ao passar o mouse","Realce neon do item selecionado"};
        const char *appearance[]={"Tonalização do dock e da barra","Reflexos e contornos do shell","Sombras do shell"};
        bool states[] = {s->prefs.animations,s->prefs.zoom,s->prefs.neon};
        if(s->settings_category==1){states[0]=s->prefs.tint;states[1]=s->prefs.reflection;states[2]=s->prefs.shadow;}
        for(int i=0;i<3;i++){
            double y=310+i*100;
            material(cr,635,y,695,80,16,false,s->glass);
            text(cr,s->settings_category ? appearance[i] : dock[i],655,y+15,18,true,0xf2eeff);
            text(cr,states[i]?"Ativado":"Desativado",655,y+44,15,false,states[i]?0x7ee7ff:0xb5abc9);
            material(cr,1245,y+26,58,27,13,states[i],s->glass);
            cairo_set_source_rgb(cr,.9,.95,1);circle(cr,states[i]?1289:1259,y+39,10);cairo_fill(cr);
        }
        text(cr,"Preferências salvas para as próximas sessões.",640,650,16,false,0xb2c2e8);
    }else{
        const char *actions[]={"Som e volume","Pasta pessoal","Monitor do sistema"};
        for(int i=0;i<3;i++){
            material(cr,635,300+i*90,695,72,16,false,s->glass);
            text(cr,actions[i],655,322+i*90,20,true,0xf1edff);
            text(cr,"›",1285,313+i*90,30,false,0x9edaff);
        }
        const char *power[]={"Encerrar sessão","Reiniciar","Desligar"};
        for(int i=0;i<3;i++){
            material(cr,635+i*235,610,225,70,16,false,s->glass);
            text(cr,power[i],650+i*235,635,17,true,i==2?0xff7fbb:0xf0efff);
        }
    }
    if(s->message[0]){text(cr,s->message,640,730,14,false,0xffb6d7);}
}
static void controls(struct futurista *s, cairo_t *cr, int width, int height) {
    material(cr, 1194, 66, 462, 632, 38, false, s->glass);
    cairo_set_source_rgba(cr, .85, .85, 1, .8);
    roundrect(cr, 1407, 83, 47, 6, 3);
    cairo_fill(cr);
    const char *labels[] = {"Wi-Fi", "Bluetooth", "Modo Avião"};
    const char *states[] = {s->wifi == 1   ? "Conectado"
                            : s->wifi == 0 ? "Desconectado"
                                           : "Indisponível",
                            s->bluetooth_busy ? "Aguarde…" : s->bluetooth == 1 ? "Ativado" : s->bluetooth == 0 ? "Desativado" : "Indisponível", "Indisponível"};
    for (int i = 0; i < 3; i++) {
        double x = 1223 + i * 145;
        material(cr, x, 125, 124, 120, 26, (i == 0 && s->wifi == 1) || (i == 1 && s->bluetooth == 1), s->glass);
        line_icon(cr, i, x + 45, 143, 34, 0xffffff);
        text(cr, labels[i], x + (i == 2 ? 16 : 24), i==1?185:194, 17, true, 0xffffff);
        text(cr, states[i], x + 12, i==1?207:218, 14, false, 0xd8daee);
        if(i==1){text(cr,"Dispositivos ›",x+12,228,12,false,0xd8daee);}
    }
    for (int i = 0; i < 2; i++) {
        double y = 272 + i * 72;
        material(cr, 1220, y, 417, 65, 24, false, s->glass);
        line_icon(cr, i == 0 ? 5 : 11, 1236, y + 15, 32, 0xffffff);
        double value = i == 0 ? s->brightness : s->volume;
        double shown = value < 0 ? 0 : value;
        material(cr, 1287, y + 17, 339, 31, 16, false, s->glass);
        if (shown > 0) {
            material(cr, 1287, y + 17, 339 * fmin(shown, 1), 31, 16, true, s->glass);
        }
        cairo_set_source_rgb(cr, .97, .97, 1);
        circle(cr, 1287 + 339 * fmin(shown, 1), y + 32, 12);
        cairo_fill(cr);
        if (value < 0) {
            text(cr, i == 0 ? "Brilho indisponível" : "Áudio indisponível", 1320, y + 23, 12, false,
                 0xd7daed);
        }
    }
    cairo_set_source_rgba(cr, .7, .67, .9, .27);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 1224, 429);
    cairo_line_to(cr, 1635, 429);
    cairo_stroke(cr);
    text(cr, "Dispositivos", 1223, 445, 19, true, 0xffffff);
    material(cr, 1221, 474, 416, 76, 24, false, s->glass);
    line_icon(cr, 7, 1245, 496, 32, 0xffffff);
    text(cr, "Exibição", 1302, 493, 17, false, 0xffffff);
    char display[64];
    snprintf(display, sizeof(display), "%d × %d", width, height);
    text(cr, display, 1302, 518, 14, false, 0xd2d8ee);
    text(cr, "›", 1603, 493, 30, false, 0xffffff);
    const char *power[] = {"Encerrar", "Reiniciar", "Desligar"};
    for (int i = 0; i < 3; i++) {
        double x = 1222 + i * 145;
        material(cr, x, 574, 124, 101, 27, false, s->glass);
        line_icon(cr, 8 + i, x + 46, 592, 32, i == 2 ? 0xff779d : 0xffffff);
        text(cr, power[i], x + 25, 630, 14, true, 0xffffff);
        if (i == 0) {
            text(cr, "Sessão", x + 32, 649, 14, true, 0xffffff);
        }
    }
    if (s->message[0]) {
        cairo_save(cr);
        roundrect(cr, 1210, 704, 444, 74, 15);
        cairo_set_source_rgba(cr, .06, .04, .16, .97);
        cairo_fill(cr);
        cairo_rectangle(cr, 1220, 712, 425, 56);
        cairo_clip(cr);
        text(cr, s->message, 1224, 718, 13, false, 0xf8dfff);
        cairo_restore(cr);
    }
}
struct layout {
    double scale, x, y;
};
static struct layout layout(int width, int height) {
    double scale = fmin(width / 1672., fmax(1, height - 52) / 889.);
    return (struct layout){scale, (width - 1672 * scale) / 2,
                           52 + (height - 52 - 889 * scale) / 2 - 52 * scale};
}
struct ui_rect futurista_dock_bounds(int width,int height){
    struct layout l=layout(width,height);
    return (struct ui_rect){l.x+(836-345)*l.scale,l.y+(469-345)*l.scale,690*l.scale,690*l.scale};
}
void futurista_overlay(struct futurista *s, cairo_t *cr, int width, int height, bool control_panel) {
    struct layout l = layout(width, height);
    cairo_save(cr);
    cairo_translate(cr, l.x, l.y);
    cairo_scale(cr, l.scale, l.scale);
    if (control_panel && s->settings_open) { settings(s,cr); cairo_restore(cr); return; }
    if (control_panel) {
        roundrect(cr, 1194, 66, 462, 632, 38);
        blurred_backdrop(s, cr, 1672, 941);
        controls(s, cr, width, height);
        cairo_restore(cr);
        return;
    }
    if (s->blurred) {
        cairo_new_path(cr);
        circle(cr, 836, 469, 326);
        blurred_backdrop(s, cr, 1672, 941);
    }
    cairo_set_source_rgba(cr, .015, .045, .14, .50);
    circle(cr, 836, 469, 325);
    cairo_fill(cr);
    static const double positions[10][2] = {{836, 216}, {994, 270}, {1077, 407}, {1076, 552}, {983, 674},
                                            {837, 714}, {690, 674}, {597, 552},  {593, 405},  {674, 277}};
    for (int i = 0; i < 10; i++) {
        sector(cr, i, s->prefs.neon ? s->hover[i] : 0, s->glass);
        double size = (i == 0 ? 86 : i == 6 ? 68 : 74) * (1 + (s->prefs.zoom ? .12 : 0) * s->hover[i]);
        double y = positions[i][1] - (s->prefs.zoom ? 5 : 0) * s->hover[i];
        if (s->glass.reflection && s->prefs.neon) { jlinux_dock_halo(cr, positions[i][0], y, size * .9, s->hover[i]); }
        app_icon(s, cr, i, positions[i][0] - size / 2, y - size / 2, size);
    }
    circle(cr,836,469,132);
    jlinux_dock_center_material(cr,s->glass);
    if (s->glass.reflection) {
        cairo_set_source_rgba(cr, .67, .28, 1, .12);
        cairo_set_line_width(cr, 12);
        cairo_stroke_preserve(cr);
        cairo_set_source_rgba(cr, .78, .40, 1, .25);
        cairo_set_line_width(cr, 4);
        cairo_stroke_preserve(cr);
        cairo_set_source_rgba(cr, .78, .28, 1, 1);
        cairo_set_line_width(cr, 1.6);
        cairo_stroke(cr);
    } else {
        cairo_new_path(cr);
    }
    app_icon(s, cr, s->selected, 788, 385, 96);
    centered_text(cr,dock_app_name(s->selected),491,25,true,0xffffff);
    centered_text(cr,s->selected==0?"Navegador Web":s->selected==2?"Editor de código":s->selected==5?"Abrir configurações":"Abrir aplicativo",525,17,false,0xb6b5d1);
    cairo_restore(cr);
}
double futurista_volume_at(double x, int width, int height) {
    struct layout l = layout(width, height);
    return l.scale > 0 ? fmin(1, fmax(0, ((x - l.x) / l.scale - 1287) / 339)) : 0;
}
int futurista_settings_hit(double x,double y,int width,int height,int category) {
    struct layout l=layout(width,height);
    if(l.scale<=0){return -1;}
    x=(x-l.x)/l.scale;y=(y-l.y)/l.scale;
    if(x>=1310 && y>=155 && y<210){return -1;}
    for(int i=0;i<3;i++){
        if(x>=380 && x<595 && y>=270+i*92 && y<336+i*92){return 100+i;}
        if(category<2 && x>=635 && x<1330 && y>=310+i*100 && y<390+i*100){return 110+category*10+i;}
        if(category==2 && x>=635 && x<1330 && y>=300+i*90 && y<372+i*90){return 130+i;}
    }
    if(category==2 && x>=635 && x<1330 && y>=610 && y<680){return 33+(int)((x-635)/235);}
    if(x<350 || x>1370 || y<140 || y>790){return -1;}
    return 40;
}
int futurista_hit(double x, double y, int width, int height, bool control_panel) {
    struct layout l = layout(width, height);
    if (l.scale <= 0) {
        return -1;
    }
    x = (x - l.x) / l.scale;
    y = (y - l.y) / l.scale;
    if (!control_panel) {
        double dx = x - 836, dy = y - 469, r = hypot(dx, dy);
        if (r < 132) {
            return 10;
        }
        if (r >= 132) {
            double a = atan2(dy, dx) * 180 / pi + 108;
            while (a < 0) {
                a += 360;
            }
            return ((int)(a / 36)) % 10;
        }
        return -1;
    }
    if (x >= 1368 && x < 1492 && y >= 224 && y < 245) { return 36; }
    if (x >= 1223 && x < 1637 && y >= 125 && y < 245) {
        return 20 + (int)((x - 1223) / 145);
    }
    if (x >= 1220 && x < 1280 && y >= 344 && y <= 409) {
        return 29;
    }
    if (x >= 1287 && x <= 1626 && y >= 344 && y <= 409) {
        return 30;
    }
    if (x >= 1221 && x < 1637 && y >= 474 && y < 550) {
        return 32;
    }
    if (x >= 1222 && x < 1637 && y >= 574 && y < 675) {
        return 33 + (int)((x - 1222) / 145);
    }
    if (x >= 1194 && x <= 1656 && y >= 66 && y <= 698) {
        return 40;
    }
    return -1;
}
static int read_number(const char *path) {
    FILE *f = fopen(path, "r");
    int n = -1;
    if (f) {
        if (fscanf(f, "%d", &n) != 1) {
            n = -1;
        }
        fclose(f);
    }
    return n;
}
void futurista_status(struct futurista *s) {
    glob_t paths = {0};
    s->battery = -1;
    s->wifi = -1;
    s->brightness = -1;
    if (!glob("/sys/class/backlight/*/brightness", 0, NULL, &paths) && paths.gl_pathc) {
        int value = read_number(paths.gl_pathv[0]);
        char maximum[512];
        snprintf(maximum, sizeof(maximum), "%.*s/max_brightness", (int)(strlen(paths.gl_pathv[0]) - 11),
                 paths.gl_pathv[0]);
        int max = read_number(maximum);
        if (value >= 0 && max > 0) {
            s->brightness = fmin(1, value / (double)max);
        }
    }
    globfree(&paths);
    memset(&paths, 0, sizeof(paths));
    struct battery_status battery = battery_read("/sys/class/power_supply");
    s->battery = battery.percent; s->charging = battery.charging;
    if (!glob("/sys/class/net/*/wireless", 0, NULL, &paths)) {
        s->wifi = 0;
        for (size_t i = 0; i < paths.gl_pathc; i++) {
            char path[512], state[24];
            snprintf(path, sizeof(path), "%.*s/operstate", (int)(strlen(paths.gl_pathv[i]) - 9),
                     paths.gl_pathv[i]);
            FILE *f = fopen(path, "r");
            if (f) {
                if (fscanf(f, "%23s", state) == 1 && !strcmp(state, "up")) {
                    s->wifi = 1;
                }
                fclose(f);
            }
        }
    }
    globfree(&paths);
}
