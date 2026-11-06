#include "../src/browser/address.h"
#include <string.h>
int main(void) {
    const char *valid[] = {"https://example.org/", "http://127.0.0.1:8080/a?q=1#b", "about:blank", "HTTPS://example.org/", "https://[::1]/"};
    const char *invalid[] = {"", "javascript:alert(1)", "data:text/html,hi", "file:///etc/passwd", "ftp://example.org/", "https://", "https://user:password@example.org", "https://example.org/\n", "https://exa mple.org", "https://example.org:99999", "about:config", "https://example.org\\@evil.org", "https://\xff.org"};
    for (size_t i = 0; i < G_N_ELEMENTS(valid); i++) { g_assert_true(browser_uri_allowed(valid[i])); }
    for (size_t i = 0; i < G_N_ELEMENTS(invalid); i++) { g_assert_false(browser_uri_allowed(invalid[i])); }
    char *uri = browser_address_uri(" example.org/path "); g_assert_cmpstr(uri, ==, "https://example.org/path"); g_free(uri);
    uri = browser_address_uri("localhost:8080"); g_assert_cmpstr(uri, ==, "https://localhost:8080"); g_free(uri);
    g_assert_null(browser_address_uri("javascript:alert(1)"));
    g_assert_null(browser_address_uri(NULL));
    char long_uri[BROWSER_ADDRESS_LIMIT + 2]; memset(long_uri, 'a', sizeof(long_uri) - 1); long_uri[sizeof(long_uri) - 1] = 0;
    g_assert_null(browser_address_uri(long_uri));
    return 0;
}
