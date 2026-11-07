#include "../src/renderer/overview.h"
#include <stdio.h>
#include <string.h>
#include <wlr/types/wlr_buffer.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Falha: %s\n",#x); return 1; } } while(0)
static bool destroyed;
static void released(struct wl_listener *listener,void *data){(void)data;destroyed=true;wl_list_remove(&listener->link);}
static void paint_buffer(struct wlr_scene_buffer *node,int x,int y,void *data){
    if(!node->buffer){return;}void *pixels;uint32_t format;size_t stride;
    if(!wlr_buffer_begin_data_ptr_access(node->buffer,WLR_BUFFER_DATA_PTR_ACCESS_READ,&pixels,&format,&stride)){return;}
    cairo_surface_t *image=cairo_image_surface_create_for_data(pixels,CAIRO_FORMAT_ARGB32,node->buffer->width,node->buffer->height,(int)stride);
    cairo_t *cr=data;cairo_save(cr);cairo_translate(cr,x,y);
    cairo_scale(cr,node->dst_width>0?(double)node->dst_width/node->buffer->width:1,node->dst_height>0?(double)node->dst_height/node->buffer->height:1);
    cairo_set_source_surface(cr,image,0,0);cairo_paint_with_alpha(cr,node->opacity);cairo_restore(cr);
    cairo_surface_destroy(image);wlr_buffer_end_data_ptr_access(node->buffer);
}
int main(int argc,char **argv){
    struct wlr_scene *scene=wlr_scene_create();CHECK(scene);
    struct wlr_scene_tree *source=wlr_scene_tree_create(&scene->tree);CHECK(source);
    cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,640,400);cairo_t *cr=cairo_create(image);
    cairo_set_source_rgb(cr,.07,.11,.19);cairo_paint(cr);
    ui_text(cr,"JLinux · conteúdo ilustrativo",20,18,20,true,0xe0e6ff);
    for(int i=0;i<7;i++){cairo_set_source_rgba(cr,.30,.52,.85,.18+i*.05);cairo_rectangle(cr,20,65+i*42,400-i*25,24);cairo_fill(cr);}
    cairo_destroy(cr);struct wlr_scene_buffer *client=window_image_create(source,image);cairo_surface_destroy(image);CHECK(client);
    struct wl_listener listener={.notify=released};wl_signal_add(&client->buffer->events.destroy,&listener);
    struct window_overview view={0};
    struct jlinux_glass glass={.tint=true,.reflection=true,.shadow=true};
    CHECK(window_overview_create(&view,scene,NULL,(struct wlr_box){0,0,1280,720},glass));
    CHECK(window_overview_page(&view,10,0));CHECK(view.count==9);
    for(size_t i=0;i<9;i++){CHECK(window_overview_add(&view,i,"Janela de teste",source,640,400));}
    CHECK(client->buffer->n_locks==10);
    CHECK(window_overview_hit(&view,180,180)==0);CHECK(window_overview_hit(&view,3,3)==-1);
    window_overview_select(&view,2);CHECK(view.cards[2].frame->opacity==1&&view.cards[0].frame->opacity<1);
    CHECK(window_overview_page(&view,10,1));CHECK(client->buffer->n_locks==1);
    CHECK(window_overview_add(&view,0,"Outra página",source,640,400));
    CHECK(view.cards[0].box.width==560&&view.cards[0].box.height==330);
    CHECK(window_overview_hit(&view,640,390)==0);
    if(argc==3&&!strcmp(argv[1],"--preview")){
        CHECK(window_overview_page(&view,3,0));
        const char *names[]={"Terminal · exemplo","Arquivos · exemplo","Navegador · exemplo"};
        for(size_t i=0;i<3;i++){CHECK(window_overview_add(&view,i,names[i],source,640,400));}window_overview_select(&view,0);
        image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1280,720);cr=cairo_create(image);ui_wallpaper(cr,1280,720);
        wlr_scene_node_for_each_buffer(&view.tree->node,paint_buffer,cr);
        CHECK(cairo_surface_write_to_png(image,argv[2])==CAIRO_STATUS_SUCCESS);cairo_destroy(cr);cairo_surface_destroy(image);
    }
    wlr_scene_node_destroy(&source->node);CHECK(!destroyed);
    window_overview_finish(&view);CHECK(destroyed&&view.tree==NULL);
    wlr_scene_node_destroy(&scene->tree.node);ui_finish();return 0;
}
