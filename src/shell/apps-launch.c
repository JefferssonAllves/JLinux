#define _GNU_SOURCE
#include "apps.h"
#include <gio/gdesktopappinfo.h>
#include <signal.h>
#include <string.h>
static const struct {const char *name, *desktop, *icon;} apps[] = {
    {"Brave", "brave-browser.desktop", "brave-desktop"},
    {"Terminal", "foot.desktop", "foot"},
    {"Neovim", "Neovim-Code.desktop", "nvim"},
    {"Arquivos", "org.jlinux.Files.desktop", "system-file-manager"},
    {"Configurações", NULL, "preferences-system"},
    {"Roblox", "org.vinegarhq.Sober.desktop", "org.vinegarhq.Sober"},
    {"Monitor", "btop.desktop", "btop"},
    {"Spotube", "com.github.KRTirtho.Spotube.desktop", "org.jlinux.Spotube"},
    {"WhatsApp", "org.jlinux.WhatsApp.desktop", "org.jlinux.WhatsAppDock"},
    {"Netflix", "org.jlinux.Netflix.desktop", "org.jlinux.Netflix"},
};
const char *dock_app_name(int index) { return index >= 0 && index < DOCK_APP_COUNT ? apps[index].name : ""; }

const char *dock_app_desktop(int index){return index>=0&&index<DOCK_APP_COUNT?apps[index].desktop:NULL;}
const char *dock_app_icon_name(int index){return index>=0&&index<DOCK_APP_COUNT?apps[index].icon:NULL;}
bool dock_app_matches(int index,const char *id,const char *title){
    if(!id){return false;}
    if(index == 0){return !g_ascii_strcasecmp(id,"brave-browser")||!g_ascii_strcasecmp(id,"brave");}
    if(index == 1){return !strcmp(id,"org.jlinux.Terminal")||!strcmp(id,"foot");}
    if(index == 2){return !strcmp(id,"org.jlinux.Neovim")||!strcmp(id,"nvim")||!strcmp(id,"Neovim-Code");}
    if(index == 3){return !strcmp(id,"org.jlinux.Files");}
    if(index == 5){return !strcmp(id,"org.vinegarhq.Sober")||!g_ascii_strcasecmp(id,"sober");}
    if(index == 6){return !strcmp(id,"org.jlinux.Monitor");}
    if(index == 7){char *lower=g_ascii_strdown(id,-1);bool match=strstr(lower,"spotube")!=NULL;g_free(lower);return match;}
    if(index == 8){return !strcmp(id,"org.jlinux.WhatsApp")||g_str_has_prefix(id,"brave-web.whatsapp.com")||((!g_ascii_strcasecmp(id,"brave-browser")||!g_ascii_strcasecmp(id,"brave"))&&title&&strstr(title,"WhatsApp"));}
    if(index == 9){return !strcmp(id,"org.jlinux.Netflix")||g_str_has_prefix(id,"brave-www.netflix.com")||g_str_has_prefix(id,"brave-netflix.com")||((!g_ascii_strcasecmp(id,"brave-browser")||!g_ascii_strcasecmp(id,"brave"))&&title&&strstr(title,"Netflix"));}
    return false;
}
static void reset_signals(gpointer data) { (void)data; sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, NULL); }
bool dock_app_launch(int index, GError **error) {
    if (index < 0 || index >= DOCK_APP_COUNT || index == 4) { return false; }
    if (index == 1 || index == 6) {
        char *terminal[] = {"foot", "--term=xterm-256color", index == 6?"--title=Monitor do sistema":"--title=JLinux Terminal", index == 6?"--app-id=org.jlinux.Monitor":"--app-id=org.jlinux.Terminal", NULL, NULL, NULL};
        if (index == 6) { terminal[4] = "-e"; terminal[5] = "btop"; }
        return g_spawn_async(NULL, terminal, NULL, G_SPAWN_SEARCH_PATH, reset_signals, NULL, NULL, error);
    }
    GDesktopAppInfo *info = g_desktop_app_info_new(apps[index].desktop);
    if (!info) { g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Aplicativo não instalado: %s", apps[index].name); return false; }
    bool ok = g_desktop_app_info_launch_uris_as_manager(info, NULL, NULL,
        G_SPAWN_SEARCH_PATH, reset_signals, NULL, NULL, NULL, error);
    g_object_unref(info); return ok;
}
