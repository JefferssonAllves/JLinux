#define _GNU_SOURCE
#include "../src/greeter/auth.h"
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
int main(int argc, char **argv) {
    if (argc != 2) { return 2; }
    struct login_auth auth;
    if (!login_auth_open(&auth, argv[1]) || !login_auth_create(&auth, "test-user")) { return 1; }
    int prompts = 0; bool done = false;
    struct timespec start; clock_gettime(CLOCK_MONOTONIC, &start);
    while (!done) {
        struct pollfd fd = {.fd = auth.fd, .events = POLLIN | ((!auth.connected || auth.tx_size) ? POLLOUT : 0)};
        if (poll(&fd, 1, 2000) <= 0 || !login_auth_dispatch(&auth, fd.revents)) { login_auth_close(&auth); return 1; }
        if (auth.state == LOGIN_PROMPT) {
            char response[128]; snprintf(response, sizeof(response), "%s", prompts++ ? "123456" : "fake-test-password");
            if (!login_auth_reply(&auth, response)) { login_auth_close(&auth); return 1; }
            for (size_t i = 0; i < strlen(prompts == 1 ? "fake-test-password" : "123456"); i++) {
                if (response[i]) { login_auth_close(&auth); return 1; }
            }
        }
        if (auth.state == LOGIN_DONE || auth.state == LOGIN_READY) { done = true; }
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > 8) { login_auth_close(&auth); return 1; }
    }
    int result = auth.state == LOGIN_DONE ? 0 : 3;
    login_auth_close(&auth); return result;
}
