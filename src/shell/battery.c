#include "battery.h"
#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct battery_status battery_read(const char *root) {
    struct battery_status result = {.percent = -1};
    char pattern[4096];
    int length = snprintf(pattern, sizeof(pattern), "%s/*/capacity", root);
    if (length < 0 || (size_t)length >= sizeof(pattern)) { return result; }
    glob_t paths = {0};
    if (!glob(pattern, 0, NULL, &paths)) {
        for (size_t i = 0; i < paths.gl_pathc; i++) {
            FILE *file = fopen(paths.gl_pathv[i], "r"); char value[64];
            if (!file) { continue; }
            bool read = fgets(value, sizeof(value), file) != NULL; fclose(file);
            if (!read) { continue; }
            errno = 0; char *end; long percent = strtol(value, &end, 10);
            if (end == value || errno || percent < 0 || percent > 100) { continue; }
            while (isspace((unsigned char)*end)) { end++; }
            if (*end) { continue; }
            result.percent = (int)percent;
            char status[4096];
            length = snprintf(status, sizeof(status), "%.*s/status", (int)(strlen(paths.gl_pathv[i]) - 9), paths.gl_pathv[i]);
            if (length >= 0 && (size_t)length < sizeof(status) && (file = fopen(status, "r"))) {
                if (fgets(value, sizeof(value), file)) { value[strcspn(value, "\r\n")] = 0; result.charging = !strcmp(value, "Charging"); }
                fclose(file);
            }
            break;
        }
    }
    globfree(&paths); return result;
}
