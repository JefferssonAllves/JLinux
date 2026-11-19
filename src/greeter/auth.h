#ifndef JLINUX_GREETER_AUTH_H
#define JLINUX_GREETER_AUTH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum login_state { LOGIN_READY, LOGIN_WAIT, LOGIN_PROMPT, LOGIN_STARTING, LOGIN_DONE, LOGIN_FAILED };
struct login_auth {
    int fd;
    enum login_state state;
    bool secret, connected;
    char message[256];
    unsigned char tx[8192], rx[65540];
    size_t tx_size, tx_sent, rx_size;
};
bool login_auth_open(struct login_auth *auth, const char *socket_path);
bool login_auth_create(struct login_auth *auth, const char *username);
bool login_auth_reply(struct login_auth *auth, char *response);
bool login_auth_cancel(struct login_auth *auth);
bool login_auth_dispatch(struct login_auth *auth, short events);
void login_auth_close(struct login_auth *auth);
#endif
