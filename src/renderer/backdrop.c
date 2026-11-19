#include "backdrop.h"
#include "renderer.h"
#include "window-effects.h"
#include "blur.h"
#include "../effects/glass.h"
#include <drm_fourcc.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/log.h>
struct backdrop {
    struct wl_list link;
    struct wlr_scene_buffer *target,*image;
    struct wl_listener target_destroy,image_destroy;
    uint64_t hash;
};
static struct wl_list backdrops;
static bool initialized;
static bool no_input(struct wlr_scene_buffer *b,double *x,double *y){(void)b;(void)x;(void)y;return false;}
static void image_destroyed(struct wl_listener *listener,void *data){
    (void)data;struct backdrop *b=wl_container_of(listener,b,image_destroy);
    wl_list_remove(&b->image_destroy.link);b->image=NULL;
}
static void target_destroyed(struct wl_listener *listener,void *data){
    (void)data;struct backdrop *b=wl_container_of(listener,b,target_destroy);
    if(b->image){wlr_scene_node_destroy(&b->image->node);}
    wl_list_remove(&b->target_destroy.link);wl_list_remove(&b->link);free(b);
}
bool jlinux_backdrop_attach(struct wlr_scene_buffer *target){
    if(!target){return false;}
    if(!initialized){wl_list_init(&backdrops);initialized=true;}
    struct backdrop *b;wl_list_for_each(b,&backdrops,link){if(b->target==target){return true;}}
    b=calloc(1,sizeof(*b));if(!b){return false;}
    b->target=target;b->target_destroy.notify=target_destroyed;
    wl_signal_add(&target->node.events.destroy,&b->target_destroy);
    wl_list_insert(backdrops.prev,&b->link);return true;
}
struct texture_ref {struct wl_list link;struct wlr_texture *texture;};
struct capture {
    struct jlinux_renderer *renderer;
    struct wlr_scene_node *stop,*skip;
    struct wlr_render_pass *pass;
    struct wl_list textures;
    struct wlr_box region;
    uint64_t hash;
    bool stopped,failed;
};
static void hash_bytes(struct capture *c,const void *p,size_t size){const unsigned char *s=p;for(size_t i=0;i<size;i++){c->hash=(c->hash^s[i])*1099511628211ULL;}}
static void buffer_size(struct wlr_scene_buffer *b,int *w,int *h){
    *w=b->dst_width?b->dst_width:b->buffer?b->buffer->width:0;
    *h=b->dst_height?b->dst_height:b->buffer?b->buffer->height:0;
}
static void render_node(struct capture *c,struct wlr_scene_node *node,int x,int y){
    if(node==c->stop){c->stopped=true;return;}
    if(c->stopped||!node->enabled||node==c->skip){return;}
    x+=node->x;y+=node->y;
    if(node->type==WLR_SCENE_NODE_TREE){
        struct wlr_scene_tree *tree=wl_container_of(node,tree,node);struct wlr_scene_node *child;
        wl_list_for_each(child,&tree->children,link){render_node(c,child,x,y);if(c->stopped){break;}}return;
    }
    int w=0,h=0;
    struct wlr_scene_buffer *b=NULL;struct wlr_scene_rect *r=NULL;
    if(node->type==WLR_SCENE_NODE_BUFFER){b=wlr_scene_buffer_from_node(node);buffer_size(b,&w,&h);}
    else if(node->type==WLR_SCENE_NODE_RECT){r=wl_container_of(node,r,node);w=r->width;h=r->height;}
    struct wlr_box bounds={x,y,w,h},intersection;
    if(!wlr_box_intersection(&intersection,&bounds,&c->region)){return;}
    hash_bytes(c,&node,sizeof(node));hash_bytes(c,&bounds,sizeof(bounds));
    struct wlr_box dst={(x-c->region.x)/4,(y-c->region.y)/4,(w+3)/4,(h+3)/4};
    if(r){hash_bytes(c,r->color,sizeof(r->color));if(c->pass){wlr_render_pass_add_rect(c->pass,&(struct wlr_render_rect_options){.box=dst,.color={r->color[0],r->color[1],r->color[2],r->color[3]}});}return;}
    if(!b||!b->buffer){return;}
    hash_bytes(c,&b->buffer,sizeof(b->buffer));hash_bytes(c,&b->opacity,sizeof(b->opacity));hash_bytes(c,&b->src_box,sizeof(b->src_box));hash_bytes(c,&b->transform,sizeof(b->transform));
    struct wlr_scene_surface *surface=wlr_scene_surface_try_from_buffer(b);
    if(surface){hash_bytes(c,&surface->surface->current.seq,sizeof(surface->surface->current.seq));}
    if(!c->pass){return;}
    struct texture_ref *ref=calloc(1,sizeof(*ref));if(!ref){c->failed=true;return;}
    ref->texture=wlr_texture_from_buffer(c->renderer->renderer,b->buffer);
    if(!ref->texture){free(ref);c->failed=true;return;}
    wl_list_insert(&c->textures,&ref->link);
    wlr_render_pass_add_texture(c->pass,&(struct wlr_render_texture_options){.texture=ref->texture,.src_box=b->src_box,.dst_box=dst,.alpha=&b->opacity,.transform=b->transform,.filter_mode=WLR_SCALE_FILTER_BILINEAR});
}
static cairo_surface_t *capture_image(struct capture *c,struct wlr_scene_node *root){
    int w=(c->region.width+3)/4,h=(c->region.height+3)/4;
    uint64_t modifiers[]={DRM_FORMAT_MOD_LINEAR,DRM_FORMAT_MOD_INVALID};
    struct wlr_drm_format format={.format=DRM_FORMAT_ARGB8888,.len=2,.capacity=2,.modifiers=modifiers};
    struct wlr_buffer *buffer=wlr_allocator_create_buffer(c->renderer->allocator,w,h,&format);
    if(!buffer){return NULL;}
    c->pass=wlr_renderer_begin_buffer_pass(c->renderer->renderer,buffer,NULL);
    if(!c->pass){wlr_buffer_drop(buffer);return NULL;}
    wl_list_init(&c->textures);c->stopped=false;c->failed=false;
    wlr_render_pass_add_rect(c->pass,&(struct wlr_render_rect_options){.box={0,0,w,h},.color={0,0,0,0},.blend_mode=WLR_RENDER_BLEND_MODE_NONE});
    int px=0,py=0;if(root->parent){wlr_scene_node_coords(&root->parent->node,&px,&py);}
    render_node(c,root,px,py);
    bool ok=wlr_render_pass_submit(c->pass);c->pass=NULL;
    struct texture_ref *ref,*next;wl_list_for_each_safe(ref,next,&c->textures,link){wlr_texture_destroy(ref->texture);wl_list_remove(&ref->link);free(ref);}
    cairo_surface_t *image=NULL;
    if(ok&&!c->failed){
        struct wlr_texture *texture=wlr_texture_from_buffer(c->renderer->renderer,buffer);
        if(texture){
            image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,w,h);
            ok=cairo_surface_status(image)==CAIRO_STATUS_SUCCESS&&wlr_texture_read_pixels(texture,&(struct wlr_texture_read_pixels_options){.data=cairo_image_surface_get_data(image),.format=DRM_FORMAT_ARGB8888,.stride=cairo_image_surface_get_stride(image)});
            wlr_texture_destroy(texture);if(!ok){cairo_surface_destroy(image);image=NULL;}else{cairo_surface_mark_dirty(image);}
        }
    }
    wlr_buffer_drop(buffer);return image;
}
static void update_one(struct backdrop *b,struct wlr_scene *scene,struct jlinux_renderer *renderer){
    int x,y,w,h;buffer_size(b->target,&w,&h);
    bool enabled=wlr_scene_node_coords(&b->target->node,&x,&y)&&b->target->buffer&&w>0&&h>0&&w<=8192&&h<=8192;
    if(b->image){wlr_scene_node_set_enabled(&b->image->node,enabled);}
    if(!enabled){return;}
    struct capture c={.renderer=renderer,.stop=&b->target->node,.skip=b->image?&b->image->node:NULL,.region={x,y,w,h},.hash=1469598103934665603ULL};
    hash_bytes(&c,&c.region,sizeof(c.region));render_node(&c,&scene->tree.node,0,0);
    hash_bytes(&c,&b->target->buffer,sizeof(b->target->buffer));hash_bytes(&c,&b->target->opacity,sizeof(b->target->opacity));
    struct wlr_scene_surface *surface=wlr_scene_surface_try_from_buffer(b->target);
    if(surface){hash_bytes(&c,&surface->surface->current.seq,sizeof(surface->surface->current.seq));}
    uint64_t hash=c.hash;if(b->image&&b->hash==hash){return;}
    if(b->image){wlr_scene_node_set_enabled(&b->image->node,false);}
    cairo_surface_t *image=capture_image(&c,&scene->tree.node);if(!image){return;}
    c.stop=NULL;c.skip=NULL;
    cairo_surface_t *mask=capture_image(&c,&b->target->node);
    if(!mask||!jlinux_blur(image,5)){if(mask){cairo_surface_destroy(mask);}cairo_surface_destroy(image);return;}
    cairo_surface_flush(image);cairo_surface_flush(mask);
    int iw=cairo_image_surface_get_width(image),ih=cairo_image_surface_get_height(image),stride=cairo_image_surface_get_stride(image);
    unsigned char *data=cairo_image_surface_get_data(image),*alpha=cairo_image_surface_get_data(mask);
    for(int row=0;row<ih;row++)for(int col=0;col<iw;col++){
        size_t i=(size_t)row*stride+col*4;unsigned a=alpha[i+3];a=a>32?255:a*255/32;
        for(int ch=0;ch<4;ch++){data[i+ch]=(data[i+ch]*a+127)/255;}
    }
    cairo_surface_mark_dirty(image);cairo_surface_destroy(mask);
    struct wlr_scene_buffer *node=window_image_create(b->target->node.parent,image);cairo_surface_destroy(image);if(!node){return;}
    if(b->image){wlr_scene_node_destroy(&b->image->node);}
    b->image=node;b->hash=hash;b->image_destroy.notify=image_destroyed;wl_signal_add(&node->node.events.destroy,&b->image_destroy);
    node->point_accepts_input=no_input;wlr_scene_buffer_set_opacity(node,b->target->opacity);wlr_scene_node_set_position(&node->node,b->target->node.x,b->target->node.y);
    wlr_scene_buffer_set_dest_size(node,w,h);wlr_scene_node_place_below(&node->node,&b->target->node);
}
void jlinux_backdrop_update(struct wlr_scene *scene,struct jlinux_renderer *renderer){
    if(!initialized){return;}
    struct jlinux_glass effects;if(!jlinux_glass_settings(&effects)){return;}
    struct backdrop *b;wl_list_for_each(b,&backdrops,link){
        if(!effects.blur){if(b->image){wlr_scene_node_set_enabled(&b->image->node,false);}continue;}
        update_one(b,scene,renderer);
    }
}
