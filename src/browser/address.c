#include "address.h"
#include <string.h>

gboolean browser_uri_allowed(const char *text) {
    if (!text || strlen(text) > BROWSER_ADDRESS_LIMIT || !g_utf8_validate(text, -1, NULL)) { return FALSE; }
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p <= 32 || *p == 127 || *p == '\\') { return FALSE; }
    }
    if (!strcmp(text, "about:blank")) { return TRUE; }
    GUri *uri = g_uri_parse(text, G_URI_FLAGS_NONE, NULL);
    if (!uri) { return FALSE; }
    const char *scheme = g_uri_get_scheme(uri), *host = g_uri_get_host(uri);
    gboolean allowed = scheme && (!g_ascii_strcasecmp(scheme, "https") || !g_ascii_strcasecmp(scheme, "http"))
        && host && *host && !g_uri_get_userinfo(uri);
    g_uri_unref(uri);
    return allowed;
}

char *browser_address_uri(const char *text) {
    if (!text || strlen(text) > BROWSER_ADDRESS_LIMIT || !g_utf8_validate(text, -1, NULL)) { return NULL; }
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p < 32 || *p == 127 || *p == '\\') { return NULL; }
    }
    char *trimmed = g_strdup(text); g_strstrip(trimmed);
    if (!*trimmed) { g_free(trimmed); return NULL; }
    const char *scheme = g_uri_peek_scheme(trimmed);
    const char *colon = strchr(trimmed, ':');
    gboolean host_port = FALSE;
    if (colon && colon[1]) {
        const char *end = colon + 1;
        while (g_ascii_isdigit(*end)) { end++; }
        host_port = end > colon + 1 && (!*end || *end == '/' || *end == '?' || *end == '#');
    }
    gboolean address = scheme || host_port || (!strchr(trimmed, ' ') &&
        (strchr(trimmed, '.') || strchr(trimmed, '/') || trimmed[0] == '[' || !strcmp(trimmed, "localhost")));
    char *uri;
    if (address) {
        uri = !scheme || host_port ? g_strconcat("https://", trimmed, NULL) : g_strdup(trimmed);
    } else {
        char *query = g_uri_escape_string(trimmed, NULL, FALSE);
        uri = g_strconcat("https://www.google.com/search?q=", query, NULL); g_free(query);
    }
    g_free(trimmed);
    if (!browser_uri_allowed(uri)) { g_free(uri); return NULL; }
    return uri;
}


gboolean browser_whatsapp_uri(const char *uri) {
    if (!uri) { return FALSE; }
    if (g_str_has_prefix(uri,"blob:")) { uri += 5; }
    if (!browser_uri_allowed(uri)) { return FALSE; }
    GUri *parsed = g_uri_parse(uri,G_URI_FLAGS_NONE,NULL);
    if (!parsed) { return FALSE; }
    const char *host = g_uri_get_host(parsed), *scheme = g_uri_get_scheme(parsed);
    gboolean allowed = host && scheme && !g_ascii_strcasecmp(scheme,"https")
        && !g_ascii_strcasecmp(host,"web.whatsapp.com")
        && (g_uri_get_port(parsed) == -1 || g_uri_get_port(parsed) == 443);
    g_uri_unref(parsed); return allowed;
}
