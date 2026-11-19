#include "../src/shell/preferences.h"
#include <glib/gstdio.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Falha: %s\n",#x);return 1;}}while(0)
int main(void){
    char *directory=g_dir_make_tmp("jlinux-prefs-XXXXXX",NULL);CHECK(directory);
    g_setenv("XDG_CONFIG_HOME",directory,true);
    struct shell_preferences p=shell_preferences_default();GError *error=NULL;
    CHECK(shell_preferences_load(&p,&error) && !error && p.animations);
    p.animations=false;p.neon=false;p.shadow=false;
    CHECK(shell_preferences_save(&p,&error) && !error);
    struct shell_preferences loaded=shell_preferences_default();
    CHECK(shell_preferences_load(&loaded,&error) && !loaded.animations && !loaded.neon && !loaded.shadow && loaded.zoom);
    char *file=g_build_filename(directory,"jlinux","shell.ini",NULL);
    GStatBuf st;CHECK(!g_stat(file,&st) && (st.st_mode&0777)==0600);
    CHECK(g_file_set_contents(file,"[Shell]\nanimations=true\nzoom=invalid\n",-1,&error));
    CHECK(!shell_preferences_load(&loaded,&error) && error && !loaded.animations && loaded.zoom);
    g_clear_error(&error);g_unlink(file);g_free(file);
    char *parent=g_build_filename(directory,"jlinux",NULL);g_rmdir(parent);g_free(parent);g_rmdir(directory);g_free(directory);
    return 0;
}
