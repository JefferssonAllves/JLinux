#ifndef JLINUX_BROWSER_BLOCKER_H
#define JLINUX_BROWSER_BLOCKER_H
#include <wpe/webkit.h>
WebKitUserContentFilter *browser_blocker_load(GError **error);
#endif
