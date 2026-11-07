#include "window-effects.h"
#include "window-frame.h"
#include "../effects/window.h"
#include <drm_fourcc.h>
#include <stdlib.h>
#include <math.h>
#include <wlr/interfaces/wlr_buffer.h>
struct effect_buffer {struct wlr_buffer base;cairo_surface_t *image;};
static void destroy_buffer(struct wlr_buffer *base){struct effect_buffer *b=wl_container_of(base,b,base);wlr_buffer_finish(base);cairo_surface_destroy(b->image);free(b);}
static bool pixels(struct wlr_buffer *base,uint32_t flags,void **data,uint32_t *format,size_t *stride){
    struct effect_buffer *b=wl_container_of(base,b,base);if(flags&WLR_BUFFER_DATA_PTR_ACCESS_WRITE){return false;}
    *data=cairo_image_surface_get_data(b->image);*format=DRM_FORMAT_ARGB8888;*stride=cairo_image_surface_get_stride(b->image);return true;
}
static void end_pixels(struct wlr_buffer *base){(void)base;}
static const struct wlr_buffer_impl impl={.destroy=destroy_buffer,.begin_data_ptr_access=pixels,.end_data_ptr_access=end_pixels};
static struct effect_buffer *buffer_new(int width,int height){
    struct effect_buffer *b=calloc(1,sizeof(*b));if(!b){return NULL;}
    b->image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
    if(cairo_surface_status(b->image)!=CAIRO_STATUS_SUCCESS){cairo_surface_destroy(b->image);free(b);return NULL;}
    wlr_buffer_init(&b->base,&impl,width,height);return b;
}
static bool no_input(struct wlr_scene_buffer *buffer,double *x,double *y){(void)buffer;(void)x;(void)y;return false;}
struct wlr_scene_buffer *window_image_create(struct wlr_scene_tree *parent,cairo_surface_t *image){
    if(cairo_surface_status(image)!=CAIRO_STATUS_SUCCESS){return NULL;}
    struct effect_buffer *buffer=calloc(1,sizeof(*buffer));if(!buffer){return NULL;}
    buffer->image=cairo_surface_reference(image);cairo_surface_flush(image);
    wlr_buffer_init(&buffer->base,&impl,cairo_image_surface_get_width(image),cairo_image_surface_get_height(image));
    struct wlr_scene_buffer *node=wlr_scene_buffer_create(parent,&buffer->base);wlr_buffer_drop(&buffer->base);
    if(node){node->point_accepts_input=no_input;}return node;
}
bool window_aura_create(struct window_aura *aura,struct wlr_scene_tree *parent){
    for(int i=0;i<4;i++){
        aura->shadow[i]=wlr_scene_buffer_create(parent,NULL);aura->glow[i]=wlr_scene_buffer_create(parent,NULL);
        if(!aura->shadow[i]||!aura->glow[i]){return false;}
        aura->shadow[i]->point_accepts_input=no_input;aura->glow[i]->point_accepts_input=no_input;
    }return true;
}
bool window_aura_update(struct window_aura *aura,int width,int height,struct jlinux_glass effects){
    if(width<1||height<1||width>8192||height>8192){return false;}
    if(aura->width==width&&aura->height==height&&aura->shadow_enabled==effects.shadow&&aura->glow_enabled==effects.reflection&&aura->tint==effects.tint){return true;}
    int pad=WINDOW_AURA_PAD,w=width+2*JLINUX_FRAME_BORDER,h=height+JLINUX_FRAME_HEADER+JLINUX_FRAME_BORDER;
    const int boxes[4][4]={{-pad,-pad,w+2*pad,pad},{-pad,0,pad,h},{w,0,pad,h},{-pad,h,w+2*pad,pad}};
    struct effect_buffer *buffers[2][4]={{0}};
    for(int layer=0;layer<2;layer++){
        bool enabled=layer==0?effects.shadow:effects.reflection;
        if(!enabled){continue;}
        for(int i=0;i<4;i++){
            buffers[layer][i]=buffer_new(boxes[i][2],boxes[i][3]);if(!buffers[layer][i]){goto fail;}
            cairo_t *cr=cairo_create(buffers[layer][i]->image);cairo_translate(cr,-boxes[i][0],-boxes[i][1]);
            window_aura_paint(cr,(struct ui_rect){0,0,w,h},layer==0,effects.tint);
            bool valid=cairo_status(cr)==CAIRO_STATUS_SUCCESS;cairo_destroy(cr);if(!valid){goto fail;}
            cairo_surface_flush(buffers[layer][i]->image);
        }
    }
    for(int layer=0;layer<2;layer++)for(int i=0;i<4;i++){
        struct wlr_scene_buffer *node=layer==0?aura->shadow[i]:aura->glow[i];
        wlr_scene_buffer_set_buffer(node,buffers[layer][i]?&buffers[layer][i]->base:NULL);
        wlr_scene_node_set_position(&node->node,boxes[i][0]-JLINUX_FRAME_BORDER,boxes[i][1]-JLINUX_FRAME_HEADER);
        if(buffers[layer][i]){wlr_buffer_drop(&buffers[layer][i]->base);}
    }
    aura->width=width;aura->height=height;aura->shadow_enabled=effects.shadow;aura->glow_enabled=effects.reflection;aura->tint=effects.tint;return true;
fail:
    for(int layer=0;layer<2;layer++){for(int i=0;i<4;i++){if(buffers[layer][i]){wlr_buffer_drop(&buffers[layer][i]->base);}}}
    return false;
}
void window_aura_opacity(struct window_aura *aura,bool focused,double lift,double visible){
    for(int i=0;i<4;i++){
        wlr_scene_buffer_set_opacity(aura->shadow[i],visible*(focused?.8:.5));
        wlr_scene_buffer_set_opacity(aura->glow[i],visible*(focused?.45:.15)*(1-lift)+visible*lift);
    }
}
struct snapshot_part {struct wl_list link;struct wlr_scene_buffer *node;float opacity;int x,y,width,height;};
struct window_snapshot {struct wlr_scene_tree *tree;struct wl_list parts;size_t count;int x,y,base_x,base_y;bool failed;};
static void clone_buffer(struct wlr_scene_buffer *source,int x,int y,void *data){
    struct window_snapshot *s=data;if(s->failed||!source->buffer){return;}
    struct snapshot_part *part=calloc(1,sizeof(*part));if(!part){s->failed=true;return;}
    part->node=wlr_scene_buffer_create(s->tree,source->buffer);
    if(!part->node){free(part);s->failed=true;return;}
    part->node->point_accepts_input=no_input;part->opacity=source->opacity;
    wlr_scene_buffer_set_source_box(part->node,source->src_box.width > 0 && source->src_box.height > 0 ? &source->src_box : NULL);
    wlr_scene_buffer_set_dest_size(part->node,source->dst_width,source->dst_height);
    wlr_scene_buffer_set_transform(part->node,source->transform);
    wlr_scene_buffer_set_filter_mode(part->node,source->filter_mode);
    wlr_scene_buffer_set_transfer_function(part->node,source->transfer_function);
    wlr_scene_buffer_set_primaries(part->node,source->primaries);
    wlr_scene_buffer_set_color_encoding(part->node,source->color_encoding);
    wlr_scene_buffer_set_color_range(part->node,source->color_range);
    wlr_scene_buffer_set_opacity(part->node,part->opacity);
    wlr_scene_node_coords(&source->node,&x,&y);
    part->x=x-s->x;part->y=y-s->y;
    bool rotated=source->transform==WL_OUTPUT_TRANSFORM_90||source->transform==WL_OUTPUT_TRANSFORM_270||source->transform==WL_OUTPUT_TRANSFORM_FLIPPED_90||source->transform==WL_OUTPUT_TRANSFORM_FLIPPED_270;
    part->width=source->dst_width>0?source->dst_width:(rotated?source->buffer->height:source->buffer->width);
    part->height=source->dst_height>0?source->dst_height:(rotated?source->buffer->width:source->buffer->height);
    wlr_scene_node_set_position(&part->node->node,part->x,part->y);
    wl_list_insert(s->parts.prev,&part->link);s->count++;
}
struct window_snapshot *window_snapshot_create(struct wlr_scene_tree *source,struct wlr_scene_tree *parent){
    struct window_snapshot *s=calloc(1,sizeof(*s));if(!s){return NULL;}wl_list_init(&s->parts);
    s->tree=wlr_scene_tree_create(parent);if(!s->tree){free(s);return NULL;}
    int parent_x,parent_y;wlr_scene_node_coords(&parent->node,&parent_x,&parent_y);
    wlr_scene_node_coords(&source->node,&s->x,&s->y);
    s->base_x=s->x-parent_x;s->base_y=s->y-parent_y;
    wlr_scene_node_set_position(&s->tree->node,s->base_x,s->base_y);
    // shortcut: propagate acquire fences before enabling an explicit-sync protocol in this compositor.
    wlr_scene_node_for_each_buffer(&source->node,clone_buffer,s);
    if(s->failed||!s->count){window_snapshot_destroy(s);return NULL;}return s;
}
void window_snapshot_opacity(struct window_snapshot *s,double visible){
    struct snapshot_part *p;wl_list_for_each(p,&s->parts,link){wlr_scene_buffer_set_opacity(p->node,p->opacity*visible);}
}
void window_snapshot_apply(struct window_snapshot *s,double visible,int offset_y){
    window_snapshot_opacity(s,visible);
    wlr_scene_node_set_position(&s->tree->node,s->base_x,s->base_y+offset_y);
}
void window_snapshot_geometry(struct window_snapshot *s,struct wlr_box from,struct wlr_box to,double progress){
    if(!s||from.width<1||from.height<1||to.width<1||to.height<1){return;}
    if(progress<0){progress=0;}if(progress>1){progress=1;}
    double sx=(from.width+(to.width-from.width)*progress)/from.width;
    double sy=(from.height+(to.height-from.height)*progress)/from.height;
    wlr_scene_node_set_position(&s->tree->node,(int)lround(from.x+(to.x-from.x)*progress),(int)lround(from.y+(to.y-from.y)*progress));
    struct snapshot_part *p;wl_list_for_each(p,&s->parts,link){
        int x=(int)lround(p->x*sx),y=(int)lround(p->y*sy);
        int width=(int)lround((p->x+p->width)*sx)-x,height=(int)lround((p->y+p->height)*sy)-y;
        wlr_scene_node_set_position(&p->node->node,x,y);
        wlr_scene_buffer_set_dest_size(p->node,width>0?width:1,height>0?height:1);
    }
}
size_t window_snapshot_count(struct window_snapshot *s){return s?s->count:0;}
void window_snapshot_destroy(struct window_snapshot *s){
    if(!s){return;}struct snapshot_part *p,*next;wl_list_for_each_safe(p,next,&s->parts,link){wl_list_remove(&p->link);free(p);}
    wlr_scene_node_destroy(&s->tree->node);free(s);
}
bool window_hint_create(struct window_hint *hint,struct wlr_scene_tree *parent){
    hint->tree=wlr_scene_tree_create(parent);if(!hint->tree){return false;}
    for(int i=0;i<5;i++){
        struct effect_buffer *buffer=buffer_new(1,1);if(!buffer){goto fail;}
        cairo_t *cr=cairo_create(buffer->image);cairo_set_source_rgba(cr,i? .53:.27,i?.49:.33,i?.99:.77,1);cairo_paint(cr);cairo_destroy(cr);cairo_surface_flush(buffer->image);
        hint->parts[i]=wlr_scene_buffer_create(hint->tree,&buffer->base);wlr_buffer_drop(&buffer->base);
        if(!hint->parts[i]){goto fail;}hint->parts[i]->point_accepts_input=no_input;
    }wlr_scene_node_set_enabled(&hint->tree->node,false);return true;
fail:wlr_scene_node_destroy(&hint->tree->node);hint->tree=NULL;return false;
}
void window_hint_apply(struct window_hint *hint,struct wlr_box box,double progress){
    bool visible=progress>0&&box.width>0&&box.height>0;
    wlr_scene_node_set_enabled(&hint->tree->node,visible);if(!visible){return;}
    int inset=(int)((1-progress)*12),w=box.width-2*inset,h=box.height-2*inset;
    if(w<4||h<4){wlr_scene_node_set_enabled(&hint->tree->node,false);return;}wlr_scene_node_set_position(&hint->tree->node,box.x+inset,box.y+inset);
    const int parts[5][4]={{0,0,w,h},{0,0,w,2},{0,h-2,w,2},{0,2,2,h-4},{w-2,2,2,h-4}};
    for(int i=0;i<5;i++){wlr_scene_node_set_position(&hint->parts[i]->node,parts[i][0],parts[i][1]);wlr_scene_buffer_set_dest_size(hint->parts[i],parts[i][2],parts[i][3]);wlr_scene_buffer_set_opacity(hint->parts[i],progress*(i?.8:.16));}
}
