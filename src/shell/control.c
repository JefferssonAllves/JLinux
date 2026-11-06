#define _GNU_SOURCE
#include "control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
static bool address(struct sockaddr_un *address) {
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
    int length = snprintf(address->sun_path, sizeof(address->sun_path), "%s/jlinux-shell-control", runtime);
    return length > 0 && (size_t)length < sizeof(address->sun_path);
}
int shell_control_open(void) {
    struct sockaddr_un addr;
    if (!address(&addr)) {
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
void shell_control_close(int fd) {
    if (fd < 0) {
        return;
    }
    struct sockaddr_un addr;
    if (address(&addr)) {
        unlink(addr.sun_path);
    }
    close(fd);
}
bool shell_control_toggle(void) {
    struct sockaddr_un addr;
    if (!address(&addr)) {
        return false;
    }
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return false;
    }
    bool sent = sendto(fd, "toggle", 6, MSG_NOSIGNAL, (struct sockaddr *)&addr, sizeof(addr)) == 6;
    close(fd);
    return sent;
}
