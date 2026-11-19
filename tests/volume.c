#define _POSIX_C_SOURCE 200809L
#include "../src/shell/volume.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Falha: %s\n", #x); return 1; } } while (0)
int main(void) {
    const char *outputs[] = {"Volume: 0.40\n", "Volume: 0.00 [MUTED]\n", "Volume: 1.25\n",
        "Volume: nan\n", "Volume: -0.4\n", "Volume: 0.4 junk\n", "invalid", "", "Volume: inf\n"};
    for (unsigned int i = 0; i < sizeof(outputs) / sizeof(*outputs); i++) {
        int fds[2]; CHECK(pipe(fds) == 0);
        CHECK(write(fds[1], outputs[i], strlen(outputs[i])) == (ssize_t)strlen(outputs[i])); close(fds[1]);
        struct volume v = {.fd = fds[0], .query = true};
        volume_complete(&v, 0);
        CHECK(v.available == (i < 3));
        if (i == 0) { CHECK(v.level == .4 && !v.muted); }
        if (i == 1) { CHECK(v.level == 0 && v.muted); }
        CHECK(v.fd == -1 && !v.pid);
    }
    struct volume pending = {.pid = 123, .available = true};
    CHECK(!volume_set(&pending, NAN));
    CHECK(!pending.pending);
    CHECK(volume_set(&pending, .2) && pending.target == .2);
    CHECK(volume_set(&pending, .73) && pending.target == .73 && pending.level == .73);
    CHECK(volume_set(&pending, 2) && pending.target == 1);
    CHECK(volume_set(&pending, -1) && pending.target == 0);
    CHECK(pending.pid == 123 && pending.pending);
    return 0;
}
