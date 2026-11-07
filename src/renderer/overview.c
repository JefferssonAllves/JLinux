#include "overview.h"
#include "blur.h"
#include "window-frame.h"
#include <drm_fourcc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wlr/render/allocator.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/transform.h>

/* One composed frame per opening, downsampled before readback; no capture timer. */
static cairo_surface_t *capture(struct wlr_scene *scene,struct wlr_output *output,int width,int height){
    struct wlr_scene_output *scene_output=wlr_scene_get_scene_output(scene,output);
    if(!scene_output||width<1||height<1||output->width>8192||output->height>8192){return NULL;}
    struct wlr_output_state state;wlr_output_state_init(&state);
    struct wlr_buffer *small=NULL;struct wlr_texture *texture=NULL,*sample=NULL;cairo_surface_t *image=NULL;
    wlr_output_lock_attach_render(output,true);
    wlr_damage_ring_add_whole(&scene_output->damage_ring);
    bool built=wlr_scene_output_build_state(scene_output,&state,NULL);
    wlr_output_lock_attach_render(output,false);
    if(!built||!state.buffer||!output->swapchain){goto done;}
    int w=(width+3)/4,h=(height+3)/4;
    small=wlr_allocator_create_buffer(output->allocator,w,h,&output->swapchain->format);
    texture=wlr_texture_from_buffer(output->renderer,state.buffer);
    if(!small||!texture){goto done;}
    struct wlr_render_pass *pass=wlr_renderer_begin_buffer_pass(output->renderer,small,NULL);
    if(!pass){goto done;}
    wlr_render_pass_add_texture(pass,&(struct wlr_render_texture_options){.texture=texture,.dst_box={0,0,w,h},.transform=wlr_output_transform_invert(output->transform),.blend_mode=WLR_RENDER_BLEND_MODE_NONE});
    if(!wlr_render_pass_submit(pass)){goto done;}
    sample=wlr_texture_from_buffer(output->renderer,small);if(!sample){goto done;}
    image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,w,h);
    if(cairo_surface_status(image)!=CAIRO_STATUS_SUCCESS||!wlr_texture_read_pixels(sample,&(struct wlr_texture_read_pixels_options){.data=cairo_image_surface_get_data(image),.format=DRM_FORMAT_ARGB8888,.stride=(uint32_t)cairo_image_surface_get_stride(image)})){
        cairo_surface_destroy(image);image=NULL;goto done;
    }
    for(int y=0;y<h;y++){uint32_t *row=(uint32_t *)(cairo_image_surface_get_data(image)+y*cairo_image_surface_get_stride(image));for(int x=0;x<w;x++){row[x]|=0xff000000;}}
    cairo_surface_mark_dirty(image);
    if(!jlinux_blur(image,7)){cairo_surface_destroy(image);image=NULL;}
done:
    if(sample){wlr_texture_destroy(sample);}if(texture){wlr_texture_destroy(texture);}if(small){wlr_buffer_drop(small);}
    wlr_output_state_finish(&state);return image;
}
static void clear_cards(struct window_overview *view){
    for(size_t i=0;i<OVERVIEW_PAGE;i++){
        window_snapshot_destroy(view->cards[i].snapshot);
        if(view->cards[i].tree){wlr_scene_node_destroy(&view->cards[i].tree->node);}
        view->cards[i]=(struct overview_card){0};
    }
    if(view->header){wlr_scene_node_destroy(&view->header->node);view->header=NULL;}
}
bool window_overview_create(struct window_overview *view,struct wlr_scene *scene,struct wlr_output *output,struct wlr_box bounds,struct jlinux_glass glass){
    *view=(struct window_overview){.bounds=bounds,.glass=glass};
    if(bounds.width<320||bounds.height<360||bounds.width>8192||bounds.height>8192){return false;}
    cairo_surface_t *image=glass.blur?capture(scene,output,bounds.width,bounds.height):NULL;view->blurred=image!=NULL;
    if(!image){image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);}
    cairo_t *cr=cairo_create(image);
    cairo_set_source_rgba(cr,.025,.035,.075,view->blurred?.44:glass.blur?1:.78);cairo_paint(cr);cairo_destroy(cr);
    view->tree=wlr_scene_tree_create(&scene->tree);
    if(!view->tree){cairo_surface_destroy(image);return false;}
    wlr_scene_node_set_position(&view->tree->node,bounds.x,bounds.y);
    struct wlr_scene_buffer *background=window_image_create(view->tree,image);cairo_surface_destroy(image);
    if(!background){window_overview_finish(view);return false;}
    wlr_scene_buffer_set_dest_size(background,bounds.width,bounds.height);
    if(glass.blur){pixman_region32_t opaque;pixman_region32_init_rect(&opaque,0,0,bounds.width,bounds.height);wlr_scene_buffer_set_opaque_region(background,&opaque);pixman_region32_fini(&opaque);}
    return true;
}
bool window_overview_page(struct window_overview *view,size_t total,size_t page){
    clear_cards(view);view->total=total;view->page=page;
    size_t remaining=total>page*OVERVIEW_PAGE?total-page*OVERVIEW_PAGE:0;
    view->count=remaining>OVERVIEW_PAGE?OVERVIEW_PAGE:remaining;
    cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,view->bounds.width,88);
    cairo_t *cr=cairo_create(image);char text[160];
    if(total){snprintf(text,sizeof(text),"Janelas · %zu abertas · página %zu/%zu",total,page+1,(total+OVERVIEW_PAGE-1)/OVERVIEW_PAGE);}
    else{snprintf(text,sizeof(text),"Nenhuma janela neste espaço");}
    ui_text(cr,text,32,16,22,true,0xeaf0ff);
    ui_text(cr,(view->blurred||!view->glass.blur)?"Clique ou Enter para abrir · Tab/setas para selecionar · Roda para páginas · Esc para voltar":"Desfoque indisponível · Clique para abrir · Esc para voltar",32,51,13,false,0xbccae2);

    cairo_destroy(cr);view->header=window_image_create(view->tree,image);cairo_surface_destroy(image);
    return view->header!=NULL;
}
bool window_overview_add(struct window_overview *view,size_t index,const char *title,struct wlr_scene_tree *source,int width,int height){
    if(index>=view->count){return false;}
    int cols=view->count<=3?(int)view->count:view->count==4?2:3;
    int rows=((int)view->count+cols-1)/cols;
    int gap=20,card_w=(view->bounds.width-64-(cols-1)*gap)/cols,card_h=(view->bounds.height-128-(rows-1)*gap)/rows;
    if(card_w>560){card_w=560;}if(card_h>330){card_h=330;}
    if(card_w<1||card_h<50){return false;}
    int row_items=(int)view->count-(int)(index/cols)*cols;if(row_items>cols){row_items=cols;}
    int origin_x=(view->bounds.width-row_items*card_w-(row_items-1)*gap)/2;
    int origin_y=96+(view->bounds.height-128-rows*card_h-(rows-1)*gap)/2;
    struct overview_card *card=&view->cards[index];
    card->box=(struct wlr_box){origin_x+(int)(index%cols)*(card_w+gap),origin_y+(int)(index/cols)*(card_h+gap),card_w,card_h};
    card->tree=wlr_scene_tree_create(view->tree);if(!card->tree){return false;}
    wlr_scene_node_set_position(&card->tree->node,card->box.x,card->box.y);
    cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,card_w,card_h);
    cairo_t *cr=cairo_create(image);struct ui_rect rect={1,1,card_w-2,card_h-2};
    cairo_set_source_rgba(cr,.05,.07,.12,.94);cairo_paint(cr);
    if(view->glass.tint){jlinux_glass_window_tint(cr,rect,8);}
    if(view->glass.shadow){jlinux_glass_window_shading(cr,rect,8);}
    if(view->glass.reflection){jlinux_glass_window_reflections(cr,rect,8,36);}
    cairo_save(cr);cairo_rectangle(cr,14,card_h-32,card_w-28,28);cairo_clip(cr);
    ui_text(cr,title?title:"Sem título",16,card_h-28,14,true,0xeaf0ff);cairo_restore(cr);
    cairo_destroy(cr);card->frame=window_image_create(card->tree,image);cairo_surface_destroy(image);
    if(!card->frame){return false;}
    card->snapshot=width>0&&height>0?window_snapshot_create(source,card->tree):NULL;
    if(card->snapshot&&width>0&&height>0){
        double scale=fmin((card_w-40.)/(width+2*JLINUX_FRAME_BORDER),(card_h-64.)/(height+JLINUX_FRAME_HEADER+JLINUX_FRAME_BORDER));
        if(scale>0){window_snapshot_geometry(card->snapshot,(struct wlr_box){0,0,width,height},(struct wlr_box){(int)((card_w-width*scale)/2),16+(int)(JLINUX_FRAME_HEADER*scale),(int)fmax(1,width*scale),(int)fmax(1,height*scale)},1);}
    }
    return true;
}
void window_overview_select(struct window_overview *view,size_t index){
    for(size_t i=0;i<view->count;i++){if(view->cards[i].frame){wlr_scene_buffer_set_opacity(view->cards[i].frame,i==index?1:.48);}}
}
int window_overview_hit(struct window_overview *view,double x,double y){
    x-=view->bounds.x;y-=view->bounds.y;
    for(size_t i=0;i<view->count;i++){if(wlr_box_contains_point(&view->cards[i].box,x,y)){return (int)i;}}return -1;
}
void window_overview_finish(struct window_overview *view){
    clear_cards(view);if(view->tree){wlr_scene_node_destroy(&view->tree->node);}*view=(struct window_overview){0};
}
