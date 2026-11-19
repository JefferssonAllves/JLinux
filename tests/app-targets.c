#include "../src/shell/apps.h"
#include <assert.h>
#include <string.h>
int main(void){
 const struct {int index;const char *id;} cases[]={
 {0,"brave-browser"},{0,"Brave-browser"},{1,"foot"},{1,"org.jlinux.Terminal"},
 {2,"org.jlinux.Neovim"},
 {3,"org.jlinux.Files"},{5,"org.vinegarhq.Sober"},{5,"sober"},
 {6,"org.godotengine.Godot"},{6,"Godot"},{7,"org-spotube-spotube-MainKt"},
 {7,"com.github.KRTirtho.Spotube"},{8,"org.jlinux.WhatsApp"},
 {8,"brave-web.whatsapp.com__-Default"},{9,"org.jlinux.Netflix"},{9,"brave-www.netflix.com__-Default"}};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){assert(dock_app_matches(cases[i].index,cases[i].id,"Test"));}
 assert(!dock_app_matches(0,"brave-web.whatsapp.com__-Default","WhatsApp"));
 assert(dock_app_matches(8,"brave-browser","WhatsApp"));
 assert(!dock_app_matches(1,"org.jlinux.Neovim","Neovim"));
 assert(!dock_app_matches(6,"foot","JLinux Terminal"));
 assert(!dock_app_matches(6,"org.jlinux.Monitor","Monitor"));
 assert(!strcmp(dock_app_desktop(6),"org.godotengine.Godot.desktop"));
 assert(!strcmp(dock_app_icon_name(6),"godot"));
 assert(!dock_app_matches(4,"org.jlinux.Files","Files"));
 assert(!dock_app_matches(0,NULL,"Brave"));
 assert(dock_app_matches(9,"brave-browser","Netflix"));
 assert(!dock_app_matches(9,"brave-browser","WhatsApp"));
 assert(!dock_app_matches(0,"brave-www.netflix.com__-Default","Netflix"));
 const char *names[]={"Brave","Terminal","Neovim","Arquivos","Configurações","Roblox","Godot","Spotube","WhatsApp","Netflix"};
 assert(sizeof(names)/sizeof(names[0])==DOCK_APP_COUNT);
 for(int i=0;i<DOCK_APP_COUNT;i++){assert(!strcmp(dock_app_name(i),names[i]));}
 return 0;
}
