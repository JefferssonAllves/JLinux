#include "chrome.h"
#include <EGL/egl.h>
#include <glib-unix.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <wpe/webkit.h>
#include <wpe/headless/wpe-headless.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

struct app {
    GMainLoop *loop;
    WebKitWebView *web;
    struct browser_chrome chrome;
    gboolean probe;
    gboolean consumed[768];
    guint consumed_buttons;
    gint64 started, load_started;
    int status;
};
static gboolean quit(gpointer data) { g_main_loop_quit(((struct app *)data)->loop); return G_SOURCE_CONTINUE; }
static void notice(struct app *app, const char *message) {
    g_strlcpy(app->chrome.message, message, sizeof(app->chrome.message));
    if (!app->probe) { app->chrome.visible = TRUE; browser_chrome_update(&app->chrome); }
}
static void show_address(struct app *app) {
    const char *uri = webkit_web_view_get_uri(app->web);
    g_strlcpy(app->chrome.address, uri && strcmp(uri, "about:blank") ? uri : "", sizeof(app->chrome.address));
    app->chrome.selected = TRUE; app->chrome.visible = TRUE; app->chrome.message[0] = 0;
    browser_chrome_update(&app->chrome);
}
static void navigate(struct app *app) {
    char *uri = browser_address_uri(app->chrome.address);
    if (!uri) { notice(app, "Use um endereço HTTP ou HTTPS válido, sem usuário/senha na URL."); return; }
    app->chrome.visible = FALSE; app->chrome.message[0] = 0; browser_chrome_update(&app->chrome);
    webkit_web_view_load_uri(app->web, uri); g_free(uri);
}
static void append_text(struct app *app, const char *text) {
    if (!text || !g_utf8_validate(text, -1, NULL)) { return; }
    for (const char *p = text; *p; p = g_utf8_next_char(p)) {
        if (!g_unichar_isprint(g_utf8_get_char(p))) { return; }
    }
    size_t offset = app->chrome.selected ? 0 : strlen(app->chrome.address);
    if (strlen(text) > BROWSER_ADDRESS_LIMIT - offset) { notice(app, "Endereço longo demais."); return; }
    g_strlcpy(app->chrome.address + offset, text, sizeof(app->chrome.address) - offset);
    app->chrome.selected = FALSE; app->chrome.message[0] = 0; browser_chrome_update(&app->chrome);
}
static gboolean view_event(WPEView *view, WPEEvent *event, gpointer data) {
    struct app *app = data; struct browser_chrome *chrome = &app->chrome;
    WPEEventType type = wpe_event_get_event_type(event);
    if (type == WPE_EVENT_POINTER_UP) {
        guint button = wpe_event_pointer_button_get_button(event);
        if (button < 32 && (app->consumed_buttons & (1u << button))) {
            app->consumed_buttons &= ~(1u << button); return TRUE;
        }
    }
    if (type == WPE_EVENT_KEYBOARD_KEY_UP) {
        guint code = wpe_event_keyboard_get_keycode(event);
        if (code < G_N_ELEMENTS(app->consumed) && app->consumed[code]) { app->consumed[code] = FALSE; return TRUE; }
        return FALSE;
    }
    if (type == WPE_EVENT_KEYBOARD_KEY_DOWN) {
        guint key = wpe_event_keyboard_get_keyval(event), code = wpe_event_keyboard_get_keycode(event);
        WPEModifiers modifiers = wpe_event_get_modifiers(event);
        gboolean ctrl = modifiers & WPE_MODIFIER_KEYBOARD_CONTROL;
        gboolean alt = modifiers & WPE_MODIFIER_KEYBOARD_ALT;
        if (ctrl && (key == XKB_KEY_l || key == XKB_KEY_L)) { show_address(app); }
        else if (ctrl && (key == XKB_KEY_q || key == XKB_KEY_Q)) { quit(app); }
        else if (key == XKB_KEY_F5 || (ctrl && (key == XKB_KEY_r || key == XKB_KEY_R))) { webkit_web_view_reload(app->web); }
        else if (alt && key == XKB_KEY_Left) { webkit_web_view_go_back(app->web); }
        else if (alt && key == XKB_KEY_Right) { webkit_web_view_go_forward(app->web); }
        else if (!chrome->visible) { return FALSE; }
        else if (key == XKB_KEY_Escape) { chrome->visible = FALSE; browser_chrome_update(chrome); }
        else if (key == XKB_KEY_Return || key == XKB_KEY_KP_Enter) { navigate(app); }
        else if (ctrl && (key == XKB_KEY_a || key == XKB_KEY_A)) { chrome->selected = TRUE; browser_chrome_update(chrome); }
        else if (ctrl && (key == XKB_KEY_v || key == XKB_KEY_V)) {
            char *text = wpe_clipboard_read_text(wpe_display_get_clipboard(wpe_view_get_display(view)), "text/plain;charset=utf-8", NULL);
            append_text(app, text); g_free(text);
        }
        else if (key == XKB_KEY_BackSpace || key == XKB_KEY_Delete) {
            char *end = chrome->selected ? chrome->address : g_utf8_find_prev_char(chrome->address, chrome->address + strlen(chrome->address));
            if (end) { *end = 0; } chrome->selected = FALSE; browser_chrome_update(chrome);
        } else if (!ctrl && !alt) {
            gunichar character = xkb_keysym_to_utf32(key);
            if (character && g_unichar_isprint(character)) {
                char text[7] = {0}; g_unichar_to_utf8(character, text); append_text(app, text);
            }
        }
        if (code < G_N_ELEMENTS(app->consumed)) { app->consumed[code] = TRUE; }
        return TRUE;
    }
    double x, y;
    if (chrome->visible && wpe_event_get_position(event, &x, &y)) {
        int width = CLAMP(wpe_view_get_width(view) - 32, 320, 4096);
        if (x >= 16 && x < width + 16 && y >= 16 && y < 128) {
            if (type == WPE_EVENT_POINTER_DOWN) {
                guint button = wpe_event_pointer_button_get_button(event);
                if (button < 32) { app->consumed_buttons |= 1u << button; }
            }
            if (type == WPE_EVENT_POINTER_DOWN && wpe_event_pointer_button_get_button(event) == WPE_BUTTON_PRIMARY) {
                if (y < 72 && x < 61) { webkit_web_view_go_back(app->web); }
                else if (y < 72 && x < 99) { webkit_web_view_go_forward(app->web); }
                else if (y < 72 && x < 143) { webkit_web_view_reload(app->web); }
                else { chrome->selected = TRUE; browser_chrome_update(chrome); }
            }
            return TRUE;
        }
        if (type == WPE_EVENT_POINTER_DOWN) {
            guint button = wpe_event_pointer_button_get_button(event);
            if (button < 32) { app->consumed_buttons |= 1u << button; }
            chrome->visible = FALSE; browser_chrome_update(chrome); return TRUE;
        }
    }
    return FALSE;
}
static void changed_title(WebKitWebView *web, GParamSpec *spec, gpointer data) {
    (void)spec; struct app *app = data;
    if (app->probe) { return; }
    const char *title = webkit_web_view_get_title(web);
    char *label = g_strdup_printf("%s — JLinux Browser", title && *title ? title : "Nova página");
    wpe_toplevel_set_title(wpe_view_get_toplevel(webkit_web_view_get_wpe_view(web)), label); g_free(label);
}
static void js_finished(GObject *object, GAsyncResult *result, gpointer data) {
    struct app *app = data; GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &error);
    if (!value || !jsc_value_to_boolean(value)) {
        g_printerr("JavaScript/DOM: %s\n", error ? error->message : "resultado inválido"); app->status = 1;
    } else {
        g_print("{\"engine\":\"WPE WebKit %u.%u.%u\",\"startup_to_ready_ms\":%.3f,\"load_ms\":%.3f,\"javascript\":true}\n",
            webkit_get_major_version(), webkit_get_minor_version(), webkit_get_micro_version(),
            (g_get_monotonic_time() - app->started) / 1000.0, (g_get_monotonic_time() - app->load_started) / 1000.0);
    }
    g_clear_object(&value); g_clear_error(&error); quit(app);
}
static void load_changed(WebKitWebView *web, WebKitLoadEvent event, gpointer data) {
    struct app *app = data;
    if (event == WEBKIT_LOAD_STARTED) { app->load_started = g_get_monotonic_time(); }
    if (event != WEBKIT_LOAD_FINISHED || app->status) { return; }
    if (app->probe) {
        webkit_web_view_evaluate_javascript(web,
            "document.readyState === 'complete' && !!document.body && (6 * 7 === 42)", -1, NULL, NULL, NULL, js_finished, app);
    } else { g_print("Página carregada (%.1f ms)\n", (g_get_monotonic_time() - app->load_started) / 1000.0); }
}
static gboolean load_failed(WebKitWebView *web, WebKitLoadEvent event, const char *uri, GError *error, gpointer data) {
    (void)web; (void)event; (void)uri; struct app *app = data;
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)) { return TRUE; }
    g_printerr("Falha de carregamento: %s\n", error->message);
    if (app->probe) { app->status = 1; quit(app); }
    else { notice(app, "Não foi possível abrir a página. Confira o endereço e a conexão."); }
    return TRUE;
}
static gboolean policy(WebKitWebView *web, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data) {
    (void)web; struct app *app = data;
    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        webkit_policy_decision_ignore(decision); notice(app, "Janelas adicionais ainda não estão disponíveis."); return TRUE;
    }
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        const char *uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(action));
        if (!browser_uri_allowed(uri)) {
            webkit_policy_decision_ignore(decision); notice(app, "Este endereço não é permitido nesta versão experimental.");
            if (app->probe) { app->status = 1; quit(app); } return TRUE;
        }
    }
    if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE
        && !webkit_response_policy_decision_is_mime_type_supported(WEBKIT_RESPONSE_POLICY_DECISION(decision))) {
        webkit_policy_decision_ignore(decision); notice(app, "Downloads ainda não estão disponíveis.");
        if (app->probe) { app->status = 1; quit(app); } return TRUE;
    }
    return FALSE;
}
static gboolean permission(WebKitWebView *web, WebKitPermissionRequest *request, gpointer data) {
    (void)web; webkit_permission_request_deny(request);
    notice(data, "Permissão recusada: controles de câmera, microfone e localização ainda indisponíveis."); return TRUE;
}
static gboolean authenticate(WebKitWebView *web, WebKitAuthenticationRequest *request, gpointer data) {
    (void)web; webkit_authentication_request_cancel(request);
    notice(data, "Autenticação HTTP ainda não disponível nesta versão."); return TRUE;
}
static void download(WebKitNetworkSession *session, WebKitDownload *item, gpointer data) {
    (void)session; webkit_download_cancel(item); notice(data, "Downloads ainda não estão disponíveis.");
}
static void terminated(WebKitWebView *web, WebKitWebProcessTerminationReason reason, gpointer data) {
    (void)web; struct app *app = data; g_printerr("Processo web encerrado: %d\n", reason);
    if (app->probe) { app->status = 1; quit(app); }
    else { notice(app, "A página parou. Use Ctrl+R para tentar novamente."); }
}
static void closed(WPEView *view, gpointer data) { (void)view; quit(data); }
static void disconnected(WPEDisplay *display, GError *error, gpointer data) {
    (void)display; struct app *app = data; app->status = 1;
    g_printerr("Conexão gráfica encerrada: %s\n", error ? error->message : "sem detalhes"); quit(app);
}
static void resized(WPEView *view, gpointer data) { (void)view; browser_chrome_update(&((struct app *)data)->chrome); }
static gboolean timeout(gpointer data) {
    struct app *app = data; g_printerr("Tempo excedido no teste do motor web\n"); app->status = 1; quit(app); return G_SOURCE_CONTINUE;
}
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        g_print("Uso: jlinux-browser [URL] | --probe URL | --preview arquivo.png\nCtrl+L endereço; Ctrl+R recarregar; Alt+←/→ histórico; Ctrl+Q sair.\n"); return 0;
    }
    if (g_getenv("WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS")) {
        g_printerr("Remova WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS: a sandbox é obrigatória.\n"); return 1;
    }
    struct app app = {.started = g_get_monotonic_time()};
    wl_list_init(&app.chrome.buffers);
    if (!jlinux_glass_settings(&app.chrome.glass)) { return 1; }
    if (argc == 3 && !strcmp(argv[1], "--preview")) {
        g_strlcpy(app.chrome.address, "https://example.org", sizeof(app.chrome.address));
        cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1000, 112);
        cairo_t *cr = cairo_create(image); browser_chrome_paint(&app.chrome, cr, 1000); cairo_destroy(cr);
        cairo_status_t result = cairo_surface_write_to_png(image, argv[2]); cairo_surface_destroy(image);
        ui_finish(); return result == CAIRO_STATUS_SUCCESS ? 0 : 1;
    }
    app.probe = argc == 3 && !strcmp(argv[1], "--probe");
    if (argc > 2 && !app.probe) {
        g_printerr("Uso: jlinux-browser [URL] | --probe URL | --preview arquivo.png\n"); return 2;
    }
    char *uri = browser_address_uri(app.probe ? argv[2] : argc == 2 ? argv[1] : "about:blank");
    if (!uri) { g_printerr("Endereço inválido: use HTTP ou HTTPS.\n"); return 2; }
    // Never fall back to DRM: the browser must not acquire a VT or replace the desktop.
    WPEDisplay *display = app.probe ? wpe_display_headless_new() : wpe_display_wayland_new();
    GError *error = NULL;
    if (!wpe_display_connect(display, &error)) {
        g_printerr("Não foi possível conectar ao display: %s\n", error->message);
        g_clear_error(&error); g_object_unref(display); g_free(uri); return 1;
    }
    app.loop = g_main_loop_new(NULL, FALSE);
    WebKitNetworkSession *session = webkit_network_session_new_ephemeral();
    webkit_network_session_set_tls_errors_policy(session, WEBKIT_TLS_ERRORS_POLICY_FAIL);
    app.web = g_object_new(WEBKIT_TYPE_WEB_VIEW, "display", display, "network-session", session, NULL);
    WPEView *view = webkit_web_view_get_wpe_view(app.web);
    g_signal_connect(display, "disconnected", G_CALLBACK(disconnected), &app);
    g_signal_connect(view, "closed", G_CALLBACK(closed), &app);
    g_signal_connect(app.web, "notify::title", G_CALLBACK(changed_title), &app);
    g_signal_connect(app.web, "load-changed", G_CALLBACK(load_changed), &app);
    g_signal_connect(app.web, "load-failed", G_CALLBACK(load_failed), &app);
    g_signal_connect(app.web, "decide-policy", G_CALLBACK(policy), &app);
    g_signal_connect(app.web, "permission-request", G_CALLBACK(permission), &app);
    g_signal_connect(app.web, "authenticate", G_CALLBACK(authenticate), &app);
    g_signal_connect(app.web, "web-process-terminated", G_CALLBACK(terminated), &app);
    g_signal_connect(session, "download-started", G_CALLBACK(download), &app);
    if (!app.probe) {
        if (!browser_chrome_init(&app.chrome, view)) {
            g_printerr("Não foi possível criar a barra nativa Wayland.\n"); app.status = 1; goto cleanup;
        }
        wpe_toplevel_resize(wpe_view_get_toplevel(view), 1100, 700);
        g_signal_connect(view, "event", G_CALLBACK(view_event), &app);
        g_signal_connect(view, "resized", G_CALLBACK(resized), &app);
        wpe_toplevel_set_title(wpe_view_get_toplevel(view), "JLinux Browser");
        show_address(&app);
        if (strcmp(uri, "about:blank")) { g_strlcpy(app.chrome.address, uri, sizeof(app.chrome.address)); }
    }
    guint term = g_unix_signal_add(SIGTERM, quit, &app), interrupt = g_unix_signal_add(SIGINT, quit, &app);
    guint deadline = app.probe ? g_timeout_add_seconds(20, timeout, &app) : 0;
    webkit_web_view_load_uri(app.web, uri);
    g_main_loop_run(app.loop);
    if (deadline) { g_source_remove(deadline); }
    g_source_remove(term); g_source_remove(interrupt);
cleanup:
    g_signal_handlers_disconnect_by_data(view, &app);
    g_signal_handlers_disconnect_by_data(app.web, &app);
    g_signal_handlers_disconnect_by_data(session, &app);
    g_signal_handlers_disconnect_by_data(display, &app);
    browser_chrome_finish(&app.chrome);
    g_object_unref(app.web); g_object_unref(session);
    // shortcut: WPE 2.54 disconnects Wayland before EGL teardown; remove when upstream fixes disposal order.
    if (!app.probe) {
        EGLDisplay egl = wpe_display_get_egl_display(display, NULL);
        if (egl != EGL_NO_DISPLAY) { eglTerminate(egl); }
    }
    g_object_unref(display);
    g_main_loop_unref(app.loop); g_free(uri);
    return app.status;
}
