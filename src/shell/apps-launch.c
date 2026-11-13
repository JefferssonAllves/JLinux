#define _GNU_SOURCE
#include "apps.h"
#include <gio/gdesktopappinfo.h>
#include <signal.h>
#include <string.h>
static const struct {const char *name, *desktop, *icon;} apps[] = {
    {"Brave", "brave-browser.desktop", "brave-desktop"},
    {"Terminal", "foot.desktop", "foot"},
    {"Neovim", "Neovim-Code.desktop", "nvim"},
    {"qBittorrent", "org.qbittorrent.qBittorrent.desktop", "qbittorrent"},
    {"Arquivos", "org.jlinux.Files.desktop", "system-file-manager"},
    {"Configurações", NULL, "preferences-system"},
    {"Imagens", "org.jlinux.Files.desktop", "folder-pictures"},
    {"Vídeos", "org.jlinux.Files.desktop", "folder-videos"},
    {"Monitor", "btop.desktop", "btop"},
    {"Spotube", "com.github.KRTirtho.Spotube.desktop", "org.jlinux.Spotube"},
    {"WhatsApp", "org.jlinux.WhatsApp.desktop", "org.jlinux.WhatsApp"},
};
const char *dock_app_name(int index) { return index >= 0 && index < 11 ? apps[index].name : ""; }

const char *dock_app_desktop(int index){return index>=0&&index<11?apps[index].desktop:NULL;}
const char *dock_app_icon_name(int index){return index>=0&&index<11?apps[index].icon:NULL;}
int dock_app_key(int index){return index==6||index==7?4:index;}
bool dock_app_matches(int index,const char *id,const char *title){
    if(!id){return false;}index=dock_app_key(index);
    if(index==0){return !g_ascii_strcasecmp(id,"brave-browser")||!g_ascii_strcasecmp(id,"brave");}
    if(index==1){return !strcmp(id,"org.jlinux.Terminal")||!strcmp(id,"foot");}
    if(index==2){return !strcmp(id,"org.jlinux.Neovim")||!strcmp(id,"nvim")||!strcmp(id,"Neovim-Code");}
    if(index==3){return !g_ascii_strcasecmp(id,"org.qbittorrent.qBittorrent")||!g_ascii_strcasecmp(id,"qbittorrent");}
    if(index==4){return !strcmp(id,"org.jlinux.Files");}
    if(index==8){return !strcmp(id,"org.jlinux.Monitor");}
    if(index==9){char *lower=g_ascii_strdown(id,-1);bool match=strstr(lower,"spotube")!=NULL;g_free(lower);return match;}
    if(index==10){return !strcmp(id,"org.jlinux.WhatsApp")||g_str_has_prefix(id,"brave-web.whatsapp.com")||((!g_ascii_strcasecmp(id,"brave-browser")||!g_ascii_strcasecmp(id,"brave"))&&title&&strstr(title,"WhatsApp"));}
    return false;
}
static void reset_signals(gpointer data) { (void)data; sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, NULL); }
bool dock_app_launch(int index, GError **error) {
    if (index < 0 || index >= 11 || index == 5) { return false; }
    if (index == 1 || index == 8) {
        char *terminal[] = {"foot", "--term=xterm-256color", index==8?"--title=Monitor do sistema":"--title=JLinux Terminal", index==8?"--app-id=org.jlinux.Monitor":"--app-id=org.jlinux.Terminal", NULL, NULL, NULL};
        if (index == 8) { terminal[4] = "-e"; terminal[5] = "btop"; }
        return g_spawn_async(NULL, terminal, NULL, G_SPAWN_SEARCH_PATH, reset_signals, NULL, NULL, error);
    }
    GDesktopAppInfo *info = g_desktop_app_info_new(apps[index].desktop);
    if (!info) { g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Aplicativo não instalado: %s", apps[index].name); return false; }
    GList *files = NULL;
    if (index == 6 || index == 7) {
        const char *directory = g_get_user_special_dir(index == 6 ? G_USER_DIRECTORY_PICTURES : G_USER_DIRECTORY_VIDEOS);
        files = g_list_append(files, g_file_new_for_path(directory ? directory : g_get_home_dir()));
    }
    GList *uris = files ? g_list_append(NULL, g_file_get_uri(files->data)) : NULL;
    bool ok = g_desktop_app_info_launch_uris_as_manager(info, uris, NULL,
        G_SPAWN_SEARCH_PATH, reset_signals, NULL, NULL, NULL, error);
    g_list_free_full(uris, g_free);
    g_list_free_full(files, g_object_unref); g_object_unref(info); return ok;
}
