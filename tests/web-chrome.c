#include "../src/browser/chrome.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#define CHECK(t) do {if(!(t)){fprintf(stderr,"Falha: %s\n",#t);return 1;}}while(0)
int main(int argc,char **argv){
    struct browser_chrome chrome={.tabs=g_ptr_array_new(),.glass={.reflection=true,.tint=true}};
    const int widths[]={240,400,800,1000,1920};
    for(guint w=0;w<G_N_ELEMENTS(widths);w++){
        double previous=180;
        for(guint count=1;count<=256;count++){
            g_ptr_array_set_size(chrome.tabs,count);chrome.active_tab=count-1;
            double tab=browser_chrome_tab_width(widths[w],count);
            CHECK(tab>0 && tab<=previous);previous=tab;
            CHECK(browser_chrome_new_tab_x(&chrome,widths[w])+32<=widths[w]);
            for(guint i=0;i<count;i++){
                gboolean close;double x=12+i*tab+fmin(tab/2,8);
                CHECK(browser_chrome_tab_at(&chrome,widths[w],x,&close)==i);CHECK(!close);
            }
            gboolean close;CHECK(browser_chrome_tab_at(&chrome,widths[w],browser_chrome_new_tab_x(&chrome,widths[w])+8,&close)==G_MAXUINT);
        }
    }
    CHECK(browser_chrome_url_at(1000,145,70));CHECK(!browser_chrome_url_at(1000,145,12));CHECK(!browser_chrome_url_at(1000,950,70));
    g_ptr_array_set_size(chrome.tabs,12);chrome.active_tab=2;chrome.editing=true;chrome.caret_visible=true;
    g_strlcpy(chrome.address,"Pesquisar no Google",sizeof(chrome.address));
    cairo_surface_t *image=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1000,BROWSER_CHROME_HEIGHT);
    cairo_t *cr=cairo_create(image);browser_chrome_paint(&chrome,cr,1000);CHECK(cairo_status(cr)==CAIRO_STATUS_SUCCESS);
    cairo_surface_flush(image);unsigned char *pixels=cairo_image_surface_get_data(image);size_t bytes=cairo_image_surface_get_stride(image)*BROWSER_CHROME_HEIGHT;
    unsigned char *with_caret=g_memdup2(pixels,bytes);chrome.caret_visible=false;browser_chrome_paint(&chrome,cr,1000);cairo_surface_flush(image);
    CHECK(memcmp(with_caret,pixels,bytes));g_free(with_caret);
    if(argc==3 && !strcmp(argv[1],"--preview")){chrome.caret_visible=true;browser_chrome_paint(&chrome,cr,1000);CHECK(cairo_surface_write_to_png(image,argv[2])==CAIRO_STATUS_SUCCESS);}
    cairo_destroy(cr);cairo_surface_destroy(image);g_ptr_array_unref(chrome.tabs);ui_finish();return 0;
}
