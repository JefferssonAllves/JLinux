#define _POSIX_C_SOURCE 200809L
#include "locale.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
bool jlinux_session_locale(const char *path) {
    if (getenv("LANG")) { return true; }
    char line[512], locale[256] = "C.UTF-8";
    FILE *file = fopen(path, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            char *value = line;
            while (isspace((unsigned char)*value)) { value++; }
            if (strncmp(value, "LANG=", 5)) { continue; }
            value += 5;
            while (isspace((unsigned char)*value)) { value++; }
            char quote = *value == '\'' || *value == '"' ? *value++ : 0;
            size_t length = 0;
            while (value[length] && (isalnum((unsigned char)value[length]) || strchr("_.@-", value[length]))) { length++; }
            char *tail = value + length;
            if (quote && *tail++ != quote) { continue; }
            while (isspace((unsigned char)*tail)) { tail++; }
            if (length && length < sizeof(locale) && (!*tail || *tail == '#')) {
                memcpy(locale, value, length); locale[length] = 0;
            }
        }
        fclose(file);
    }
    return setenv("LANG", locale, 0) == 0;
}
