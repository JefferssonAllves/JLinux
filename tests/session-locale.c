#define _POSIX_C_SOURCE 200809L
#include "../src/session/locale.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "Falha: %s\n", #test); return 1; } } while (0)
int main(void) {
    char path[] = "/tmp/jlinux-locale-XXXXXX";
    int fd = mkstemp(path); CHECK(fd >= 0);
    FILE *file = fdopen(fd, "w"); CHECK(file);
    fputs("# Locale do sistema\nLC_TIME=pt_BR.UTF-8\nLANG=\"pt_BR.UTF-8\" # idioma\n", file); fclose(file);
    unsetenv("LANG"); unsetenv("LC_ALL"); setenv("LC_TIME", "C", 1);
    CHECK(jlinux_session_locale(path)); CHECK(!strcmp(getenv("LANG"), "pt_BR.UTF-8"));
    CHECK(!strcmp(getenv("LC_TIME"), "C"));
    setenv("LANG", "en_US.UTF-8", 1);
    CHECK(jlinux_session_locale(path)); CHECK(!strcmp(getenv("LANG"), "en_US.UTF-8"));
    file = fopen(path, "w"); CHECK(file);
    fputs("LANG=$(touch /tmp/should-not-run)\nLANG='invalid space'\nLANG=\"unterminated\n", file); fclose(file);
    unsetenv("LANG");
    CHECK(jlinux_session_locale(path)); CHECK(!strcmp(getenv("LANG"), "C.UTF-8"));
    CHECK(unlink(path) == 0);
    unsetenv("LANG"); setenv("LC_ALL", "C", 1);
    CHECK(jlinux_session_locale(path)); CHECK(!strcmp(getenv("LANG"), "C.UTF-8"));
    CHECK(!strcmp(getenv("LC_ALL"), "C"));
    return 0;
}
