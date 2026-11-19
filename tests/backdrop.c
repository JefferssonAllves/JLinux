#include "../src/renderer/backdrop.h"
#include "../src/renderer/renderer.h"
#include "../src/renderer/window-effects.h"
#include <stdio.h>
#include <stdint.h>
#include <wlr/backend/headless.h>
#include <wlr/types/wlr_buffer.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Falha: %s\n",#x);return 1;}}while(0)
int main(void){
 struct wl_display *display=wl_display_create();CHECK(display);
 struct wlr_backend *backend=wlr_headless_backend_create(wl_display_get_event_loop(display));CHECK(backend);
 struct jlinux_renderer renderer={0};CHECK(jlinux_renderer_init(&renderer,backend,display));
 struct wlr_scene *scene=wlr_scene_create();CHECK(scene);
 cairo_surface_t *source=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,64,64);cairo_t *cr=cairo_create(source);
 for(int x=0;x<64;x++){cairo_set_source_rgb(cr,x%2?1:0,0,x%2?0:1);cairo_rectangle(cr,x,0,1,64);cairo_fill(cr);}cairo_destroy(cr);
 struct wlr_scene_buffer *under=window_image_create(&scene->tree,source);CHECK(under);cairo_surface_destroy(source);
 cairo_surface_t *glass=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,48,48);cr=cairo_create(glass);cairo_set_source_rgba(cr,.02,.03,.08,.65);cairo_rectangle(cr,0,0,32,48);cairo_fill(cr);cairo_destroy(cr);
 struct wlr_scene_buffer *target=window_image_create(&scene->tree,glass);CHECK(target);cairo_surface_destroy(glass);wlr_scene_node_set_position(&target->node,8,8);
 CHECK(jlinux_backdrop_attach(target));jlinux_backdrop_update(scene,&renderer);
 struct wlr_scene_node *node=wl_container_of(target->node.link.prev,node,link);CHECK(node!=&under->node);
 struct wlr_scene_buffer *blur=wlr_scene_buffer_from_node(node);CHECK(blur&&blur->buffer);CHECK(blur->dst_width==48&&blur->dst_height==48);
 void *pixels;uint32_t format;size_t stride;CHECK(wlr_buffer_begin_data_ptr_access(blur->buffer,WLR_BUFFER_DATA_PTR_ACCESS_READ,&pixels,&format,&stride));
 uint32_t color=*(uint32_t *)((unsigned char *)pixels+4*stride+3*4);CHECK((color&255)>30&&((color>>16)&255)>30);CHECK((color>>24)>240);
 CHECK(*(uint32_t *)((unsigned char *)pixels+4*stride+11*4)==0);wlr_buffer_end_data_ptr_access(blur->buffer);
 struct wlr_buffer *cached=wlr_buffer_lock(blur->buffer);jlinux_backdrop_update(scene,&renderer);CHECK(blur->buffer==cached);
 wlr_scene_node_set_position(&target->node,12,12);jlinux_backdrop_update(scene,&renderer);
 node=wl_container_of(target->node.link.prev,node,link);blur=wlr_scene_buffer_from_node(node);CHECK(blur->buffer!=cached);CHECK(node->x==12&&node->y==12);wlr_buffer_unlock(cached);
 wlr_scene_node_destroy(&scene->tree.node);jlinux_renderer_finish(&renderer);wlr_backend_destroy(backend);wl_display_destroy(display);return 0;
}
