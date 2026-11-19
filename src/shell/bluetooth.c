#define _GNU_SOURCE
#include "bluetooth.h"
#include <fcntl.h>
#include <glib.h>
#include <glob.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
bool bluetooth_start(struct bluetooth *bt, int action) {
    if (bt->pid || action < -1 || action > 1) { return false; }
    int pipes[2];
    if (pipe2(pipes,O_CLOEXEC|O_NONBLOCK)) { snprintf(bt->error,sizeof(bt->error),"Não foi possível consultar Bluetooth."); return false; }
    glob_t adapters={0}; char path[80]="/org/bluez/hci0";
    if (!glob("/sys/class/bluetooth/hci*",0,NULL,&adapters) && adapters.gl_pathc) {
        const char *name=strrchr(adapters.gl_pathv[0],'/'); snprintf(path,sizeof(path),"/org/bluez/%s",name+1);
    }
    globfree(&adapters);
    char *args[]={"busctl","--system","--timeout=3",action<0?"get-property":"set-property","org.bluez",path,"org.bluez.Adapter1","Powered",NULL,NULL,NULL};
    if(action>=0){args[8]="b";args[9]=action?"true":"false";}
    char **env=g_get_environ();env=g_environ_setenv(env,"LC_ALL","C",true);
    posix_spawn_file_actions_t files;int error=posix_spawn_file_actions_init(&files);
    if(!error){
        error=posix_spawn_file_actions_adddup2(&files,pipes[1],STDOUT_FILENO);
        if(!error){error=posix_spawn_file_actions_adddup2(&files,pipes[1],STDERR_FILENO);}
        posix_spawnattr_t attr;
        if(!error){
            error=posix_spawnattr_init(&attr);
            if(!error){
                sigset_t empty;sigemptyset(&empty);error=posix_spawnattr_setsigmask(&attr,&empty);
                if(!error){error=posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETSIGMASK);}
                if(!error){error=posix_spawnp(&bt->pid,args[0],&files,&attr,args,env);}
                posix_spawnattr_destroy(&attr);
            }
        }
        posix_spawn_file_actions_destroy(&files);
    }
    g_strfreev(env);close(pipes[1]);
    if(error){close(pipes[0]);bt->pid=0;snprintf(bt->error,sizeof(bt->error),"Serviço Bluetooth indisponível.");return false;}
    bt->fd=pipes[0];bt->action=action;bt->error[0]=0;clock_gettime(CLOCK_MONOTONIC,&bt->started);return true;
}
void bluetooth_complete(struct bluetooth *bt, int status) {
    char result[512]={0};ssize_t count=read(bt->fd,result,sizeof(result)-1);
    close(bt->fd);bt->fd=-1;bt->pid=0;
    if(!WIFEXITED(status)||WEXITSTATUS(status)){
        if(bt->action<0){bt->available=false;}
        snprintf(bt->error,sizeof(bt->error),"Bluetooth indisponível ou bloqueado. Abra Dispositivos para verificar.");return;
    }
    if(bt->action>=0){bluetooth_start(bt,-1);return;}
    bt->available=count>0 && (!strcmp(result,"b true\n")||!strcmp(result,"b false\n"));
    if(bt->available){bt->powered=!strcmp(result,"b true\n");}
    else{snprintf(bt->error,sizeof(bt->error),"Adaptador Bluetooth indisponível.");}
}
void bluetooth_finish(struct bluetooth *bt) {
    if(bt->pid){kill(bt->pid,SIGKILL);waitpid(bt->pid,NULL,0);close(bt->fd);bt->pid=0;}
}
