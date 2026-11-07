#define _GNU_SOURCE
#include "chrome.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <linux/input-event-codes.h>

struct chrome_buffer {
    struct browser_chrome *chrome;
    struct wl_buffer *wl;
    struct wl_list link;
    void *data;
    size_t size;
};
double browser_chrome_tab_width(int width, guint count) {
    return MIN(BROWSER_TAB_WIDTH, MAX(1, width - 56) / (double)MAX(1u, count));
}
static gboolean close_visible(double width, gboolean active) { return width >= 76 || (active && width >= 40); }
guint browser_chrome_tab_at(struct browser_chrome *chrome, int width, double x, gboolean *close) {
    guint count = chrome->tabs ? chrome->tabs->len : 1;
    double tab = browser_chrome_tab_width(width, count);
    *close = FALSE;
    if (x < 12 || x >= 12 + count * tab) { return G_MAXUINT; }
    guint index = (guint)((x - 12) / tab);
    double local = x - 12 - index * tab;
    if (local >= tab - MIN(4,tab*.12)) { return G_MAXUINT; }
    *close = close_visible(tab, index == chrome->active_tab) && local >= tab - 30;
    return index;
}
gboolean browser_chrome_url_at(int width, double x, double y) {
    return x >= 133 && x < width - 117 && y >= BROWSER_TAB_HEIGHT + 15 && y < BROWSER_TAB_HEIGHT + 53;
}
int browser_chrome_new_tab_x(struct browser_chrome *chrome, int width) {
    guint count = chrome->tabs ? chrome->tabs->len : 1;
    return (int)ceil(12 + count * browser_chrome_tab_width(width, count));
}
static void rounded(cairo_t *cr, double x, double y, double width, double height, double radius) {
    radius = MIN(radius, MIN(width, height)/2);
    cairo_new_sub_path(cr);
    cairo_arc(cr,x+width-radius,y+radius,radius,-G_PI/2,0);
    cairo_arc(cr,x+width-radius,y+height-radius,radius,0,G_PI/2);
    cairo_arc(cr,x+radius,y+height-radius,radius,G_PI/2,G_PI);
    cairo_arc(cr,x+radius,y+radius,radius,G_PI,3*G_PI/2); cairo_close_path(cr);
}
static PangoLayout *text_layout(cairo_t *cr, const char *text, int size) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_from_string("Sans");
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    pango_layout_set_font_description(layout, font); pango_font_description_free(font);
    pango_layout_set_single_paragraph_mode(layout, TRUE); pango_layout_set_text(layout, text, -1);
    return layout;
}
static void icon(cairo_t *cr, int kind, double x, double y) {
    cairo_save(cr); cairo_translate(cr,x,y); cairo_set_line_width(cr,1.6); cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);
    if(kind==0 || kind==1){double direction=kind?1:-1; cairo_move_to(cr,-direction*3,-6);cairo_line_to(cr,direction*3,0);cairo_line_to(cr,-direction*3,6);}
    else if(kind==2){cairo_arc(cr,0,0,7,.4,5.7);cairo_line_to(cr,7,-7);cairo_move_to(cr,6,-3);cairo_line_to(cr,2,-4);}
    else {cairo_move_to(cr,0,-7);cairo_line_to(cr,0,4);cairo_move_to(cr,-4,0);cairo_line_to(cr,0,4);cairo_line_to(cr,4,0);cairo_move_to(cr,-7,7);cairo_line_to(cr,7,7);}
    cairo_stroke(cr);cairo_restore(cr);
}
void browser_chrome_paint(struct browser_chrome *chrome, cairo_t *cr, int width) {
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_set_source_rgba(cr,.006,.009,.016,.96); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    guint count = chrome->tabs ? chrome->tabs->len : 1;
    double tab = browser_chrome_tab_width(width,count);
    gboolean unused; guint hover = chrome->pointer_inside && chrome->pointer_y < BROWSER_TAB_HEIGHT ? browser_chrome_tab_at(chrome,width,chrome->pointer_x,&unused) : G_MAXUINT;
    for(guint i=0;i<count;i++){
        double x=12+i*tab, w=MAX(.1,tab-MIN(4,tab*.12)); gboolean active=i==chrome->active_tab;
        rounded(cr,x,5,w,29,MIN(7,w/2));
        cairo_set_source_rgba(cr,.25,.31,.52,active?.36:i==hover?.22:.09);cairo_fill(cr);
        if(active && w>=8 && chrome->glass.reflection){
            cairo_pattern_t *rim=cairo_pattern_create_linear(x,0,x+w,0);
            cairo_pattern_add_color_stop_rgba(rim,0,.24,.63,1,.85);cairo_pattern_add_color_stop_rgba(rim,1,.81,.46,.87,.75);
            cairo_set_source(cr,rim);cairo_move_to(cr,x+6,5.5);cairo_line_to(cr,x+w-6,5.5);cairo_set_line_width(cr,1);cairo_stroke(cr);cairo_pattern_destroy(rim);
        }
        gboolean close=close_visible(tab,active);
        if(w>=46){
            WebKitWebView *web=chrome->tabs?g_ptr_array_index(chrome->tabs,i):NULL;
            const char *title=web?webkit_web_view_get_title(web):"Nova aba";
            if(!title || !*title){title="Nova aba";}
            char *valid=g_utf8_make_valid(title,MIN(strlen(title),512)); PangoLayout *label=text_layout(cr,valid,12);g_free(valid);
            pango_layout_set_width(label,MAX(1,(int)(w-(close?40:16)))*PANGO_SCALE);pango_layout_set_ellipsize(label,PANGO_ELLIPSIZE_END);
            cairo_set_source_rgb(cr,active?.91:.65,active?.94:.70,active?1:.80);cairo_move_to(cr,x+10,11);pango_cairo_show_layout(cr,label);g_object_unref(label);
        }else if(w>=8 && !close){cairo_set_source_rgba(cr,.53,.61,.91,active?1:.5);cairo_arc(cr,x+w/2,19,2,0,2*G_PI);cairo_fill(cr);}
        if(close){cairo_set_source_rgba(cr,.76,.82,.95,active?1:.7);cairo_set_line_width(cr,1.2);double cx=x+tab-18;cairo_move_to(cr,cx-3,16);cairo_line_to(cr,cx+3,22);cairo_move_to(cr,cx+3,16);cairo_line_to(cr,cx-3,22);cairo_stroke(cr);}
    }
    double plus=browser_chrome_new_tab_x(chrome,width)+15;
    cairo_set_source_rgb(cr,.78,.83,.94);cairo_set_line_width(cr,1.5);cairo_move_to(cr,plus-5,19);cairo_line_to(cr,plus+5,19);cairo_move_to(cr,plus,14);cairo_line_to(cr,plus,24);cairo_stroke(cr);
    cairo_save(cr);cairo_translate(cr,0,BROWSER_TAB_HEIGHT);
    for(int i=0;i<3;i++){cairo_set_source_rgb(cr,.74,.79,.89);icon(cr,i,28+i*38,34);}
    double field_width=MAX(1,width-250);
    rounded(cr,133,15,field_width,38,9);cairo_set_source_rgba(cr,.04,.06,.10,.85);cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr,chrome->error?.9:.43,chrome->error?.38:.51,chrome->error?.43:.73,chrome->editing||chrome->error?.8:.24);cairo_set_line_width(cr,1);cairo_stroke(cr);
    if(chrome->editing && chrome->glass.reflection){
        cairo_pattern_t *rim=cairo_pattern_create_linear(140,0,width-120,0);
        cairo_pattern_add_color_stop_rgba(rim,0,.30,.60,.96,.65);cairo_pattern_add_color_stop_rgba(rim,1,.77,.44,.81,.7);
        cairo_set_source(cr,rim);cairo_move_to(cr,143,15.5);cairo_line_to(cr,width-127,15.5);cairo_stroke(cr);cairo_pattern_destroy(rim);
    }
    /* The status dot indicates editing, not a security guarantee about the site. */
    cairo_set_source_rgba(cr,.51,.60,.85,chrome->editing?1:.5);cairo_arc(cr,147,34,2,0,2*G_PI);cairo_fill(cr);
    const char *text=chrome->error?chrome->message:chrome->address[0]?chrome->address:"Pesquise no Google ou digite um endereço";
    PangoLayout *layout=text_layout(cr,text,14);int text_width;pango_layout_get_pixel_size(layout,&text_width,NULL);
    double available=MAX(1,field_width-36),left=159;
    if(chrome->editing && !chrome->error && chrome->address[0]){left-=MAX(0,text_width-available+3);}
    cairo_save(cr);cairo_rectangle(cr,158,20,available,28);cairo_clip(cr);
    if(chrome->editing && !chrome->error && chrome->selected && chrome->address[0]){cairo_set_source_rgba(cr,.30,.43,.80,.45);cairo_rectangle(cr,left,24,text_width,21);cairo_fill(cr);}
    cairo_set_source_rgb(cr,chrome->address[0]?.88:.49,chrome->address[0]?.92:.55,chrome->address[0]?.99:.68);cairo_move_to(cr,left,25);pango_cairo_show_layout(cr,layout);
    if(chrome->editing && !chrome->error && chrome->caret_visible && !chrome->selected){double caret=left+(chrome->address[0]?text_width:0)+.5;cairo_set_source_rgb(cr,.84,.91,1);cairo_set_line_width(cr,1.3);cairo_move_to(cr,caret,25);cairo_line_to(cr,caret,44);cairo_stroke(cr);}
    cairo_restore(cr);g_object_unref(layout);
    cairo_set_source_rgb(cr,.76,.82,.94);
    if(chrome->download_status[0]){ui_text(cr,chrome->download_status,width-105,25,12,false,0xcbd8f0);}else{icon(cr,3,width-85,34);}
    cairo_restore(cr);
}
static void free_buffer(struct chrome_buffer *buffer) {
    wl_buffer_destroy(buffer->wl); munmap(buffer->data, buffer->size);
    wl_list_remove(&buffer->link); buffer->chrome->in_flight--; g_free(buffer);
}
static void released(void *data, struct wl_buffer *wl) {
    (void)wl; struct chrome_buffer *buffer = data; struct browser_chrome *chrome = buffer->chrome;
    free_buffer(buffer);
    if (chrome->dirty) { browser_chrome_update(chrome); }
}
static const struct wl_buffer_listener buffer_listener = {.release = released};
static gboolean draw(gpointer data) {
    struct browser_chrome *chrome = data; chrome->idle = 0;
    if (!chrome->surface) { return G_SOURCE_REMOVE; }
    if (chrome->in_flight >= 2) { return G_SOURCE_REMOVE; }
    int width = wpe_view_get_width(chrome->view);
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    size_t size = (size_t)stride * BROWSER_CHROME_HEIGHT;
    int fd = memfd_create("jlinux-browser-chrome", MFD_CLOEXEC);
    if (fd < 0) { goto failed; }
    if (ftruncate(fd, (off_t)size)) { close(fd); goto failed; }
    void *mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { close(fd); goto failed; }
    WPEDisplayWayland *display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(chrome->view));
    struct wl_shm_pool *pool = wl_shm_create_pool(wpe_display_wayland_get_wl_shm(display), fd, (int)size);
    close(fd);
    if (!pool) { munmap(mapping, size); goto failed; }
    struct wl_buffer *wl = wl_shm_pool_create_buffer(pool, 0, width, BROWSER_CHROME_HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    if (!wl) { munmap(mapping, size); goto failed; }
    struct chrome_buffer *buffer = g_new0(struct chrome_buffer, 1);
    *buffer = (struct chrome_buffer){.chrome = chrome, .wl = wl, .data = mapping, .size = size};
    wl_list_insert(&chrome->buffers, &buffer->link); chrome->in_flight++;
    wl_buffer_add_listener(wl, &buffer_listener, buffer);
    cairo_surface_t *image = cairo_image_surface_create_for_data(mapping, CAIRO_FORMAT_ARGB32, width, BROWSER_CHROME_HEIGHT, stride);
    cairo_t *cr = cairo_create(image); browser_chrome_paint(chrome, cr, width);
    gboolean valid = cairo_status(cr) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr); cairo_surface_destroy(image);
    if (!valid) { free_buffer(buffer); goto failed; }
    wl_surface_attach(chrome->surface, wl, 0, 0);
    wl_surface_damage_buffer(chrome->surface, 0, 0, width, BROWSER_CHROME_HEIGHT); wl_surface_commit(chrome->surface);
    chrome->dirty = FALSE;
    return G_SOURCE_REMOVE;
failed:
    g_warning("Não foi possível desenhar a barra de navegação");
    return G_SOURCE_REMOVE;
}
static gboolean blink_caret(gpointer data) {
    struct browser_chrome *chrome=data;
    chrome->caret_visible=!chrome->caret_visible;
    browser_chrome_update(chrome);return G_SOURCE_CONTINUE;
}
void browser_chrome_update(struct browser_chrome *chrome) {
    gboolean blink=chrome->editing && chrome->view && chrome->surface && wpe_view_get_has_focus(chrome->view);
    if(blink && !chrome->caret_timer){chrome->caret_visible=TRUE;chrome->caret_timer=g_timeout_add(530,blink_caret,chrome);}
    else if(!blink && chrome->caret_timer){g_source_remove(chrome->caret_timer);chrome->caret_timer=0;chrome->caret_visible=FALSE;}
    chrome->dirty = TRUE;
    if (chrome->surface && !chrome->idle) { chrome->idle = g_idle_add(draw, chrome); }
}
static void update_cursor(struct browser_chrome *chrome, struct wl_pointer *pointer) {
    if(!chrome->pointer_inside || !chrome->cursor_theme){return;}
    gboolean text=browser_chrome_url_at(wpe_view_get_width(chrome->view),chrome->pointer_x,chrome->pointer_y);
    struct wl_cursor *cursor=wl_cursor_theme_get_cursor(chrome->cursor_theme,text?"text":"left_ptr");
    if(!cursor && text){cursor=wl_cursor_theme_get_cursor(chrome->cursor_theme,"xterm");}
    if(!cursor || !cursor->image_count){return;}
    chrome->cursor_text=text;struct wl_cursor_image *image=cursor->images[0];
    wl_pointer_set_cursor(pointer,chrome->pointer_serial,chrome->cursor_surface,image->hotspot_x,image->hotspot_y);
    wl_surface_attach(chrome->cursor_surface,wl_cursor_image_get_buffer(image),0,0);
    wl_surface_damage(chrome->cursor_surface,0,0,image->width,image->height);wl_surface_commit(chrome->cursor_surface);
}
static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
        struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    struct browser_chrome *chrome=data;chrome->pointer_inside=surface==chrome->surface;
    chrome->pointer_serial=serial;chrome->pointer_x=wl_fixed_to_double(x);chrome->pointer_y=wl_fixed_to_double(y);
    update_cursor(chrome,pointer);browser_chrome_update(chrome);
}
static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    (void)pointer;(void)serial;(void)surface;struct browser_chrome *chrome=data;
    chrome->pointer_inside=FALSE;browser_chrome_update(chrome);
}
static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)time;struct browser_chrome *chrome=data;int width=wpe_view_get_width(chrome->view);gboolean unused;
    guint before=chrome->pointer_y<BROWSER_TAB_HEIGHT?browser_chrome_tab_at(chrome,width,chrome->pointer_x,&unused):G_MAXUINT;
    chrome->pointer_x=wl_fixed_to_double(x);chrome->pointer_y=wl_fixed_to_double(y);
    if(chrome->cursor_text!=browser_chrome_url_at(width,chrome->pointer_x,chrome->pointer_y)){update_cursor(chrome,pointer);}
    guint after=chrome->pointer_y<BROWSER_TAB_HEIGHT?browser_chrome_tab_at(chrome,width,chrome->pointer_x,&unused):G_MAXUINT;
    if(before!=after){browser_chrome_update(chrome);}
}
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
        uint32_t time, uint32_t button, uint32_t state) {
    (void)pointer; (void)serial; (void)time; struct browser_chrome *chrome = data;
    if (chrome->pointer_inside && button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED && chrome->clicked) {
        chrome->clicked(chrome, chrome->pointer_x, chrome->pointer_y, chrome->clicked_data);
    }
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data; (void)pointer; (void)time; (void)axis; (void)value;
}
static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter, .leave = pointer_leave, .motion = pointer_motion,
    .button = pointer_button, .axis = pointer_axis,
};
static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
    struct browser_chrome *chrome = data;
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !chrome->pointer) {
        chrome->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(chrome->pointer, &pointer_listener, chrome);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && chrome->pointer) {
        wl_pointer_destroy(chrome->pointer); chrome->pointer = NULL; chrome->pointer_inside = FALSE;
    }
}
static const struct wl_seat_listener seat_listener = {.capabilities = seat_capabilities};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)version; struct browser_chrome *chrome = data;
    if (!strcmp(interface, wl_subcompositor_interface.name)) {
        chrome->subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    } else if (!strcmp(interface, wl_seat_interface.name) && !chrome->seat) {
        chrome->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(chrome->seat, &seat_listener, chrome);
    }
}
static void global_remove(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};
gboolean browser_chrome_init(struct browser_chrome *chrome, WPEView *view) {
    chrome->view = view; wl_list_init(&chrome->buffers);
    WPEDisplayWayland *display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    chrome->registry = wl_display_get_registry(wpe_display_wayland_get_wl_display(display));
    if (!chrome->registry) { return FALSE; }
    wl_registry_add_listener(chrome->registry, &registry_listener, chrome);
    if (wl_display_roundtrip(wpe_display_wayland_get_wl_display(display)) < 0 || !chrome->subcompositor) { return FALSE; }
    struct wl_compositor *compositor = wpe_display_wayland_get_wl_compositor(display);
    chrome->surface = wl_compositor_create_surface(compositor);
    if (!chrome->surface) { return FALSE; }
    chrome->subsurface = wl_subcompositor_get_subsurface(chrome->subcompositor, chrome->surface,
        wpe_view_wayland_get_wl_surface(WPE_VIEW_WAYLAND(view)));
    if (!chrome->subsurface) { return FALSE; }
    // Negative child coordinates extend the automatic xdg geometry above the unchanged page origin.
    wl_subsurface_set_position(chrome->subsurface, 0, -BROWSER_CHROME_HEIGHT);
    wl_subsurface_set_desync(chrome->subsurface);
    chrome->cursor_surface = wl_compositor_create_surface(compositor);
    if (!chrome->cursor_surface) { return FALSE; }
    chrome->cursor_theme = wl_cursor_theme_load(NULL, 24, wpe_display_wayland_get_wl_shm(display));
    draw(chrome);
    return TRUE;
}
void browser_chrome_finish(struct browser_chrome *chrome) {
    if (chrome->caret_timer) { g_source_remove(chrome->caret_timer); }
    if (chrome->idle) { g_source_remove(chrome->idle); }
    if (chrome->pointer) { wl_pointer_destroy(chrome->pointer); }
    if (chrome->seat) { wl_seat_destroy(chrome->seat); }
    if (chrome->cursor_surface) { wl_surface_destroy(chrome->cursor_surface); }
    if (chrome->cursor_theme) { wl_cursor_theme_destroy(chrome->cursor_theme); }
    if (chrome->subsurface) { wl_subsurface_destroy(chrome->subsurface); }
    if (chrome->surface) { wl_surface_destroy(chrome->surface); }
    struct chrome_buffer *buffer, *tmp;
    wl_list_for_each_safe(buffer, tmp, &chrome->buffers, link) { free_buffer(buffer); }
    if (chrome->subcompositor) { wl_subcompositor_destroy(chrome->subcompositor); }
    if (chrome->registry) { wl_registry_destroy(chrome->registry); }
}
