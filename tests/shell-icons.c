#include "../src/shell/apps.h"
#include <glib/gstdio.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Falha: %s\n",#x);return 1;}}while(0)
int main(int argc,char **argv){
    (void)argv;
    if(argc>1){for(int i=0;i<11;i++){cairo_surface_t *icon=dock_app_icon(i);CHECK(icon);cairo_surface_destroy(icon);printf("Ícone instalado: %s OK\n",dock_app_name(i));}return 0;}
    char *directory=g_dir_make_tmp("jlinux-icons-XXXXXX",NULL);CHECK(directory);g_setenv("XDG_DATA_HOME",directory,true);
    const char *icons[]={"brave-desktop","foot","nvim","qbittorrent","system-file-manager","preferences-system","folder-pictures","folder-videos","btop","spotify","org.jlinux.WhatsApp"};
    char *path=g_build_filename(directory,"icons","hicolor","128x128","apps",NULL);CHECK(!g_mkdir_with_parents(path,0700));
    for(int i=0;i<11;i++){
        char *file=g_strdup_printf("%s/%s.png",path,icons[i]);cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,8,8);
        CHECK(cairo_surface_write_to_png(image,file)==CAIRO_STATUS_SUCCESS);cairo_surface_destroy(image);
        image=dock_app_icon(i);CHECK(image && cairo_image_surface_get_width(image)==(i==5?128:8));cairo_surface_destroy(image);g_unlink(file);g_free(file);
    }
    g_rmdir(path);g_free(path);path=g_strdup(directory);
    char *child=g_build_filename(path,"icons","hicolor","128x128",NULL);g_rmdir(child);g_free(child);
    child=g_build_filename(path,"icons","hicolor",NULL);g_rmdir(child);g_free(child);
    child=g_build_filename(path,"icons",NULL);g_rmdir(child);g_free(child);g_rmdir(directory);g_free(path);g_free(directory);return 0;
}
