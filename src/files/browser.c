#define _GNU_SOURCE
#include "browser.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
void browser_finish(struct browser *browser) {
    for (size_t i = 0; i < browser->count; i++) { free(browser->entries[i].name); g_free(browser->entries[i].label); }
    free(browser->entries); free(browser->path); browser->entries = NULL; browser->path = NULL; browser->count = 0;
}
static int compare(const void *a, const void *b) {
    const struct file_entry *left = a, *right = b;
    if (left->directory != right->directory) { return left->directory ? -1 : 1; }
    int order = g_utf8_collate(left->label, right->label);
    return order ? order : strcmp(left->name, right->name);
}
/* shortcut: synchronous listing; use a worker before supporting slow network mounts. */
bool browser_load(struct browser *browser, const char *path) {
    struct browser next = {.hidden = browser->hidden};
    next.path = realpath(path, NULL);
    if (!next.path) { return false; }
    DIR *directory = opendir(next.path);
    if (!directory) { free(next.path); return false; }
    size_t capacity = 0; int error = 0;
    for (;;) {
        errno = 0; struct dirent *entry = readdir(directory);
        if (!entry) { if (errno) { error = errno; } break; }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") || (!next.hidden && entry->d_name[0] == '.')) { continue; }
        struct stat info;
        if (fstatat(dirfd(directory), entry->d_name, &info, AT_SYMLINK_NOFOLLOW)) { continue; }
        if (next.count == 50000) { error = EOVERFLOW; break; }
        if (next.count == capacity) {
            size_t expanded = capacity ? capacity * 2 : 64;
            struct file_entry *entries = reallocarray(next.entries, expanded, sizeof(*entries));
            if (!entries) { error = ENOMEM; break; }
            next.entries = entries; capacity = expanded;
        }
        bool symlink = S_ISLNK(info.st_mode), folder = S_ISDIR(info.st_mode);
        if (symlink) { struct stat target; folder = !fstatat(dirfd(directory), entry->d_name, &target, 0) && S_ISDIR(target.st_mode); }
        char *name = strdup(entry->d_name);
        if (!name) { error = ENOMEM; break; }
        char *label = g_filename_display_name(name);
        g_strdelimit(label, "\r\n\t", ' ');
        next.entries[next.count++] = (struct file_entry){name, label, folder, symlink, info.st_size};
    }
    closedir(directory);
    if (error) { browser_finish(&next); errno = error; return false; }
    if (next.count > 1) { qsort(next.entries, next.count, sizeof(*next.entries), compare); }
    browser_finish(browser); *browser = next; return true;
}
char *browser_entry_path(const struct browser *browser, size_t index) {
    if (index >= browser->count || !browser->path) { errno = EINVAL; return NULL; }
    return g_build_filename(browser->path, browser->entries[index].name, NULL);
}
