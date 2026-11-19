#include "../src/renderer/window-effects.h"
#include <stdio.h>
#include <string.h>
#include "../src/renderer/window-frame.h"
#include <math.h>
#include <wlr/types/wlr_buffer.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr,"Falha: %s\n",#test); return 1; } } while (0)
static bool destroyed;
static void buffer_destroyed(struct wl_listener *listener, void *data) {
    (void)data; destroyed=true; wl_list_remove(&listener->link);
}
struct check_snapshot { int count; bool valid; };
static void check_buffer(struct wlr_scene_buffer *buffer,int x,int y,void *data) {
    struct check_snapshot *check=data; check->count++;
    double sx=0,sy=0;
    check->valid &= x>=150 && y>=180 && fabs(buffer->opacity-.08)<.001;
    check->valid &= !buffer->point_accepts_input(buffer,&sx,&sy);
}
static void paint_buffer(struct wlr_scene_buffer *buffer,int x,int y,void *data) {
    if (!buffer->buffer) { return; }
    cairo_t *cr=data; void *pixels; uint32_t format; size_t stride;
    if (!wlr_buffer_begin_data_ptr_access(buffer->buffer,WLR_BUFFER_DATA_PTR_ACCESS_READ,&pixels,&format,&stride)) { return; }
    wlr_scene_node_coords(&buffer->node,&x,&y);
    cairo_surface_t *image=cairo_image_surface_create_for_data(pixels,CAIRO_FORMAT_ARGB32,buffer->buffer->width,buffer->buffer->height,(int)stride);
    cairo_save(cr); cairo_translate(cr,x,y);
    if(buffer->dst_width && buffer->dst_height){cairo_scale(cr,(double)buffer->dst_width/buffer->buffer->width,(double)buffer->dst_height/buffer->buffer->height);}
    cairo_set_source_surface(cr,image,0,0); cairo_paint_with_alpha(cr,buffer->opacity); cairo_restore(cr);
    cairo_surface_destroy(image); wlr_buffer_end_data_ptr_access(buffer->buffer);
}
int main(int argc,char **argv) {
    struct wlr_scene *scene=wlr_scene_create(); CHECK(scene);
    struct wlr_scene_tree *parent=wlr_scene_tree_create(&scene->tree); CHECK(parent);
    wlr_scene_node_set_position(&parent->node,100,50);
    struct window_hint source={0}; CHECK(window_hint_create(&source,parent));
    window_hint_apply(&source,(struct wlr_box){50,120,640,600},1);
    /* Keep one source buffer alive only through a snapshot, after its tree dies. */
    struct wlr_buffer *tracked=source.parts[0]->buffer;
    struct wl_listener listener={.notify=buffer_destroyed};
    wl_signal_add(&source.parts[0]->buffer->events.destroy,&listener);
    struct window_snapshot *snapshot=window_snapshot_create(source.tree,parent); CHECK(snapshot);
    CHECK(window_snapshot_count(snapshot)==5); CHECK(tracked->n_locks==2);
    wlr_scene_node_destroy(&source.tree->node); CHECK(!destroyed); CHECK(tracked->n_locks==1);
    window_snapshot_apply(snapshot,.5,10);
    struct check_snapshot check={.valid=true};
    /* The fill has original opacity .16; edges .8. Inspect fill separately. */
    struct wlr_scene_node *node;
    struct wlr_scene_tree *ghost=wl_container_of(parent->children.next,ghost,node.link);
    CHECK(ghost->node.x==50 && ghost->node.y==130);
    node=wl_container_of(ghost->children.next,node,link);
    struct wlr_scene_buffer *fill=wlr_scene_buffer_from_node(node);
    int x,y; CHECK(wlr_scene_node_coords(node,&x,&y));
    check_buffer(fill,x,y,&check); CHECK(check.valid && check.count==1);
    double sx,sy; CHECK(!wlr_scene_node_at(&scene->tree.node,170,200,&sx,&sy));
    struct wlr_box from={50,120,640,600},to={250,200,320,300};
    window_snapshot_geometry(snapshot,from,to,0);
    CHECK(ghost->node.x==50 && ghost->node.y==120 && fill->dst_width==640);
    window_snapshot_geometry(snapshot,from,to,.5);
    CHECK(ghost->node.x==150 && ghost->node.y==160 && fill->dst_width==480 && fill->dst_height==450);
    window_snapshot_geometry(snapshot,from,to,1);
    CHECK(ghost->node.x==250 && ghost->node.y==200 && fill->dst_width==320 && fill->dst_height==300);
    window_snapshot_destroy(snapshot); CHECK(destroyed);
    struct window_aura aura={0}; CHECK(window_aura_create(&aura,parent));
    struct jlinux_glass effects={.shadow=true,.reflection=true,.tint=true};
    CHECK(!window_aura_update(&aura,0,600,effects));
    CHECK(window_aura_update(&aura,640,600,effects));
    size_t pixels=0;
    for(int i=0;i<4;i++){CHECK(aura.shadow[i]->buffer && aura.glow[i]->buffer); pixels+=(size_t)aura.shadow[i]->buffer->width*aura.shadow[i]->buffer->height;}
    CHECK(pixels<60000);
    window_aura_opacity(&aura,true,1,1); CHECK(aura.glow[0]->opacity==1);
    effects.shadow=false; CHECK(window_aura_update(&aura,640,600,effects));
    CHECK(!aura.shadow[0]->buffer && aura.glow[0]->buffer);
    effects.reflection=false; CHECK(window_aura_update(&aura,640,600,effects)); CHECK(!aura.glow[0]->buffer);
    struct window_hint hint={0}; CHECK(window_hint_create(&hint,parent));
    window_hint_apply(&hint,(struct wlr_box){0,0,640,600},.5);
    CHECK(hint.tree->node.enabled && hint.tree->node.x==6 && hint.parts[0]->dst_width==628);
    window_hint_apply(&hint,(struct wlr_box){0,0,1,1},.5); CHECK(!hint.tree->node.enabled);
    window_hint_apply(&hint,(struct wlr_box){0,0,640,600},0); CHECK(!hint.tree->node.enabled);
    wlr_scene_node_destroy(&scene->tree.node);
    if(argc==3 && !strcmp(argv[1],"--preview")){
        scene=wlr_scene_create(); CHECK(scene);
        struct window_hint preview={0}; CHECK(window_hint_create(&preview,&scene->tree));
        window_hint_apply(&preview,(struct wlr_box){0,36,640,684},1);
        struct wlr_scene_tree *window=wlr_scene_tree_create(&scene->tree); CHECK(window);
        wlr_scene_node_set_position(&window->node,430,180);
        struct window_aura depth={0}; CHECK(window_aura_create(&depth,window));
        effects=(struct jlinux_glass){.shadow=true,.reflection=true,.tint=true};
        CHECK(window_aura_update(&depth,640,420,effects)); window_aura_opacity(&depth,true,1,1);
        /* Flat client placeholder: component preview, not an application capture. */
        struct wlr_scene_buffer *client=wlr_scene_buffer_create(window,preview.parts[0]->buffer); CHECK(client);
        client->point_accepts_input=preview.parts[0]->point_accepts_input;
        wlr_scene_buffer_set_dest_size(client,640,420); wlr_scene_buffer_set_opacity(client,.6);
        struct jlinux_window_frame frame={0}; CHECK(jlinux_window_frame_create(&frame,window));
        CHECK(jlinux_window_frame_update(&frame,640,420,"JLinux · janela em movimento",effects,false));
        cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1280,720);
        cairo_t *cr=cairo_create(image); ui_wallpaper(cr,1280,720);
        wlr_scene_node_for_each_buffer(&scene->tree.node,paint_buffer,cr);
        ui_text(cr,"Prévia de componentes nativos · encaixe e profundidade",24,12,14,false,0xc0c9ed);
        CHECK(cairo_surface_write_to_png(image,argv[2])==CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr); cairo_surface_destroy(image); wlr_scene_node_destroy(&scene->tree.node); ui_finish();
    }
    return 0;
}
