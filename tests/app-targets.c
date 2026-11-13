#include "../src/shell/apps.h"
#include <assert.h>
int main(void){
 const struct {int index;const char *id;} cases[]={
 {0,"brave-browser"},{0,"Brave-browser"},{1,"foot"},{1,"org.jlinux.Terminal"},
 {2,"org.jlinux.Neovim"},{3,"org.qbittorrent.qBittorrent"},
 {4,"org.jlinux.Files"},{6,"org.jlinux.Files"},{7,"org.jlinux.Files"},
 {8,"org.jlinux.Monitor"},{9,"org-spotube-spotube-MainKt"},
 {9,"com.github.KRTirtho.Spotube"},{10,"org.jlinux.WhatsApp"},
 {10,"brave-web.whatsapp.com__-Default"}};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){assert(dock_app_matches(cases[i].index,cases[i].id,"Test"));}
 assert(!dock_app_matches(0,"brave-web.whatsapp.com__-Default","WhatsApp"));
 assert(dock_app_matches(10,"brave-browser","WhatsApp"));
 assert(!dock_app_matches(1,"org.jlinux.Neovim","Neovim"));
 assert(!dock_app_matches(8,"foot","JLinux Terminal"));
 assert(!dock_app_matches(5,"org.jlinux.Files","Files"));
 assert(!dock_app_matches(0,NULL,"Brave"));
 assert(dock_app_key(6)==4&&dock_app_key(7)==4);
 return 0;
}
