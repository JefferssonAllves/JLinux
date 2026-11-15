#define _GNU_SOURCE
#include "../src/files/thumbnails.h"
#include <glib/gstdio.h>
#include <unistd.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Thumbnail check failed: %s\n",#x);return 1;}}while(0)
int main(void){
    char *root=g_dir_make_tmp("jlinux-thumbs-XXXXXX",NULL);CHECK(root);
    char *path=g_build_filename(root,"photo.png",NULL),*link=g_build_filename(root,"link.png",NULL);
    cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,120,80);cairo_t *cr=cairo_create(image);
    cairo_set_source_rgb(cr,.2,.6,.9);cairo_paint(cr);CHECK(cairo_surface_write_to_png(image,path)==CAIRO_STATUS_SUCCESS);cairo_destroy(cr);cairo_surface_destroy(image);
    GHashTable *cache=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,(GDestroyNotify)cairo_surface_destroy);
    cairo_surface_t *thumb=file_thumbnail(cache,path);CHECK(thumb);CHECK(cairo_image_surface_get_width(thumb)==64);CHECK(cairo_image_surface_get_height(thumb)<64);
    CHECK(file_thumbnail(cache,path)==thumb);CHECK(!symlink(path,link));CHECK(!file_thumbnail(cache,link));
    char *bad=g_build_filename(root,"broken.png",NULL);CHECK(g_file_set_contents(bad,"not an image",-1,NULL));CHECK(!file_thumbnail(cache,bad));CHECK(!file_thumbnail(cache,bad));
    g_hash_table_destroy(cache);g_unlink(path);g_unlink(link);g_unlink(bad);g_rmdir(root);g_free(bad);g_free(link);g_free(path);g_free(root);return 0;
}
