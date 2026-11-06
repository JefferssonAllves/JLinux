#define _GNU_SOURCE
#include "../src/files/browser.h"
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "Falha: %s\n", #test); return 1; } } while (0)
int main(int argc, char **argv) {
    CHECK(argc == 2);
    struct browser browser = {0}; CHECK(browser_load(&browser, argv[1]));
    CHECK(browser.count == 6);
    CHECK(browser.entries[0].directory && browser.entries[1].directory);
    bool raw_name = false, broken = false;
    for (size_t i = 0; i < browser.count; i++) {
        CHECK(g_utf8_validate(browser.entries[i].label, -1, NULL));
        if (strchr(browser.entries[i].name, '\xff')) { raw_name = true; }
        if (!strcmp(browser.entries[i].name, "broken-link")) { broken = browser.entries[i].symlink && !browser.entries[i].directory; }
    }
    CHECK(raw_name && broken);
    char *saved = g_strdup(browser.path);
    CHECK(!browser_load(&browser, "/jlinux-test-missing-directory"));
    CHECK(!strcmp(saved, browser.path) && browser.count == 6); g_free(saved);
    char *child = browser_entry_path(&browser, 0); CHECK(child);
    CHECK(browser_load(&browser, child)); CHECK(browser.count == 0); g_free(child);
    CHECK(browser_load(&browser, argv[1]));
    browser.hidden = true; CHECK(browser_load(&browser, browser.path)); CHECK(browser.count == 7);
    errno = 0; CHECK(!browser_entry_path(&browser, browser.count)); CHECK(errno == EINVAL);
    browser_finish(&browser); CHECK(!browser.path && !browser.count);
    return 0;
}
