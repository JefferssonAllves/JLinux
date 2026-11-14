#define _GNU_SOURCE
#include "control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
static bool address(struct sockaddr_un *address,const char *name) {
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime || runtime[0] != '/') {
        return false;
    }
    struct stat st;
    if (stat(runtime, &st) || st.st_uid != getuid() || (st.st_mode & 077) || !S_ISDIR(st.st_mode)) {
        return false;
    }
    memset(address, 0, sizeof(*address));
    address->sun_family = AF_UNIX;
    int length = snprintf(address->sun_path, sizeof(address->sun_path), "%s/%s", runtime,name);
    return length > 0 && (size_t)length < sizeof(address->sun_path);
}
static int open_control(const char *name) {
    struct sockaddr_un addr;
    if (!address(&addr,name)) {
        return -1;
    }
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        close(fd);
        return -1;
    }
    if (chmod(addr.sun_path, 0600)) {
        close(fd);
        unlink(addr.sun_path);
        return -1;
    }
    return fd;
}
static void close_control(int fd,const char *name) {
    if (fd < 0) {
        return;
    }
    struct sockaddr_un addr;
    if (address(&addr,name)) {
        unlink(addr.sun_path);
    }
    close(fd);
}
bool shell_control_dock(bool visible) {
    const char *name="jlinux-shell-control";
    struct sockaddr_un addr;
    if (!address(&addr,name)) {
        return false;
    }
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return false;
    }
    bool sent = sendto(fd, visible ? "show" : "hide", 4, MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof(addr)) == 4;
    close(fd);
    return sent;
}

int shell_control_open(void){return open_control("jlinux-shell-control");}
void shell_control_close(int fd){close_control(fd,"jlinux-shell-control");}
int window_control_open(void){return open_control("jlinux-window-control");}
void window_control_close(int fd){close_control(fd,"jlinux-window-control");}
bool window_control_launch(int index){
    if(index<0||index>=11||index==5){return false;}
    struct sockaddr_un addr;if(!address(&addr,"jlinux-window-control")){return false;}
    int fd=socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(fd<0){return false;}
    unsigned char command=(unsigned char)index;
    bool sent=sendto(fd,&command,1,MSG_NOSIGNAL,(struct sockaddr *)&addr,sizeof(addr))==1;
    close(fd);return sent;
}

bool shell_control_error(const char *text){
    struct sockaddr_un addr;if(!address(&addr,"jlinux-shell-control")){return false;}
    int fd=socket(AF_UNIX,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(fd<0){return false;}
    char message[186];snprintf(message,sizeof(message),"error %s",text);
    size_t size=strlen(message);
    bool sent=sendto(fd,message,size,MSG_NOSIGNAL,(struct sockaddr *)&addr,sizeof(addr))==(ssize_t)size;close(fd);return sent;
}
