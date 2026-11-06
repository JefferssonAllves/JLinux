#ifndef JLINUX_WEB_ADDRESS_H
#define JLINUX_WEB_ADDRESS_H
#include <glib.h>
#define BROWSER_ADDRESS_LIMIT 4096
gboolean browser_uri_allowed(const char *uri);
char *browser_address_uri(const char *text);
#endif
