#ifndef JLINUX_BROWSER_H
#define JLINUX_BROWSER_H
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
struct file_entry { char *name, *label; bool directory, symlink; off_t size; };
struct browser { char *path; struct file_entry *entries; size_t count; bool hidden; };
bool browser_load(struct browser *browser, const char *path);
char *browser_entry_path(const struct browser *browser, size_t index);
void browser_finish(struct browser *browser);
#endif
