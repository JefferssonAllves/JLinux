#define _GNU_SOURCE
#include "thumbnails.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <sys/stat.h>
#include <string.h>
static cairo_surface_t *load(const char *path) {
    int width=0,height=0;
    if (!gdk_pixbuf_get_file_info(path,&width,&height) || width<=0 || height<=0 || (gint64)width*height>40000000) { return NULL; }
    GdkPixbuf *pixels=gdk_pixbuf_new_from_file_at_scale(path,64,64,TRUE,NULL);
    if (!pixels) { return NULL; }
    width=gdk_pixbuf_get_width(pixels);height=gdk_pixbuf_get_height(pixels);
    cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
    unsigned char *out=cairo_image_surface_get_data(surface),*in=gdk_pixbuf_get_pixels(pixels);
    int channels=gdk_pixbuf_get_n_channels(pixels),stride=gdk_pixbuf_get_rowstride(pixels),out_stride=cairo_image_surface_get_stride(surface);
    for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
        unsigned char *p=in+y*stride+x*channels;unsigned a=channels==4?p[3]:255;
        ((guint32 *)(out+y*out_stride))[x]=(a<<24)|((p[0]*a/255)<<16)|((p[1]*a/255)<<8)|(p[2]*a/255);
    }
    cairo_surface_mark_dirty(surface);g_object_unref(pixels);return surface;
}
cairo_surface_t *file_thumbnail(GHashTable *cache,const char *path) {
    struct stat st;
    if(lstat(path,&st)||!S_ISREG(st.st_mode)||st.st_size>16*1024*1024){return NULL;}
    char *key=g_strdup_printf("%s:%lld:%lld",path,(long long)st.st_mtime,(long long)st.st_size);
    gpointer found=NULL;
    if(g_hash_table_lookup_extended(cache,key,NULL,&found)){g_free(key);return found;}
    if(g_hash_table_size(cache)>=256){g_hash_table_remove_all(cache);}
    cairo_surface_t *surface=load(path);
    if(!surface){
        char *uri=g_filename_to_uri(path,NULL,NULL),*hash=uri?g_compute_checksum_for_string(G_CHECKSUM_MD5,uri,-1):NULL;
        if(hash){
            char *name=g_strconcat(hash,".png",NULL),*cached=g_build_filename(g_get_user_cache_dir(),"thumbnails","normal",name,NULL);
            /* Only reuse thumbnails newer than the source. */
            struct stat thumb;if(!stat(cached,&thumb)&&thumb.st_mtime>=st.st_mtime){surface=load(cached);}
            g_free(cached);g_free(name);
        }
        g_free(hash);g_free(uri);
    }
    g_hash_table_insert(cache,key,surface);return surface;
}
