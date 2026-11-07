#define _GNU_SOURCE
#include "chrome.h"
#include "blocker.h"
#include <EGL/egl.h>
#include <glib-unix.h>
#include <gst/gst.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wpe/webkit.h>
#include <wpe/headless/wpe-headless.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

struct app {
    GMainLoop *loop;
    WebKitWebView *web;
    WPEDisplay *display;
    WPEToplevel *toplevel;
    WebKitNetworkSession *session;
    GPtrArray *downloads;
    struct browser_chrome chrome;
    gboolean probe;
    gboolean media_available;
    WebKitUserContentFilter *filter;
    gboolean consumed[768];
    gint64 started, load_started;
    int status;
};
static void new_tab(struct app *app, const char *uri);
static void open_downloads(void);
static void select_tab(struct app *app, guint index);
static void close_tab(struct app *app, guint index);
static gboolean quit(gpointer data) { g_main_loop_quit(((struct app *)data)->loop); return G_SOURCE_CONTINUE; }
static void notice(struct app *app, const char *message) {
    g_strlcpy(app->chrome.message, message, sizeof(app->chrome.message));
    if (!app->probe) { browser_chrome_update(&app->chrome); }
}
static void web_notice(struct app *app, WebKitWebView *web, const char *message) {
    g_object_set_data_full(G_OBJECT(web), "jlinux-notice", g_strdup(message), g_free);
    if (web == app->web) { notice(app, message); }
}
static void show_address(struct app *app) {
    const char *uri = webkit_web_view_get_uri(app->web);
    g_strlcpy(app->chrome.address, uri && strcmp(uri, "about:blank") ? uri : "", sizeof(app->chrome.address));
    app->chrome.selected = TRUE; app->chrome.editing = TRUE;
    g_strlcpy(app->chrome.message, app->media_available ? "" : "Vídeo indisponível: instale os plugins GStreamer base/good/libav.", sizeof(app->chrome.message));
    browser_chrome_update(&app->chrome);
}
static void focus_page(struct app *app) {
    const char *uri = webkit_web_view_get_uri(app->web);
    g_strlcpy(app->chrome.address, uri && strcmp(uri, "about:blank") ? uri : "", sizeof(app->chrome.address));
    app->chrome.editing = FALSE; app->chrome.selected = FALSE;
    browser_chrome_update(&app->chrome);
}
static void navigate(struct app *app) {
    char *uri = browser_address_uri(app->chrome.address);
    if (!uri) { notice(app, "Digite uma busca ou um endereço HTTP/HTTPS válido, sem credenciais e até 4096 bytes."); return; }
    app->chrome.editing = FALSE; app->chrome.message[0] = 0; browser_chrome_update(&app->chrome);
    webkit_web_view_load_uri(app->web, uri); g_free(uri);
}
static void toggle_blocker(struct app *app) {
    if (!app->filter) { notice(app, "Bloqueio indisponível nesta execução; confira o registro de inicialização."); return; }
    gboolean enabled = !GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->web), "jlinux-adblock"));
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(app->web);
    if (enabled) { webkit_user_content_manager_add_filter(manager, app->filter); }
    else { webkit_user_content_manager_remove_filter(manager, app->filter); }
    g_object_set_data(G_OBJECT(app->web), "jlinux-adblock", GINT_TO_POINTER(enabled));
    app->chrome.adblock_enabled = enabled; browser_chrome_update(&app->chrome);
    webkit_web_view_reload(app->web);
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
        else if (ctrl && (modifiers & WPE_MODIFIER_KEYBOARD_SHIFT) && (key == XKB_KEY_b || key == XKB_KEY_B)) { toggle_blocker(app); }
        else if (ctrl && (key == XKB_KEY_j || key == XKB_KEY_J)) { open_downloads(); }
        else if (ctrl && (key == XKB_KEY_t || key == XKB_KEY_T)) { new_tab(app, "about:blank"); }
        else if (ctrl && (key == XKB_KEY_w || key == XKB_KEY_W)) { close_tab(app, chrome->active_tab); }
        else if (ctrl && (key == XKB_KEY_Tab || key == XKB_KEY_ISO_Left_Tab)) {
            guint count = chrome->tabs->len;
            gboolean previous = modifiers & WPE_MODIFIER_KEYBOARD_SHIFT;
            select_tab(app, (chrome->active_tab + (previous ? count - 1 : 1)) % count);
        }
        else if (ctrl && (key == XKB_KEY_q || key == XKB_KEY_Q)) { quit(app); }
        else if (key == XKB_KEY_F5 || (ctrl && (key == XKB_KEY_r || key == XKB_KEY_R))) { webkit_web_view_reload(app->web); }
        else if (alt && key == XKB_KEY_Left) { webkit_web_view_go_back(app->web); }
        else if (alt && key == XKB_KEY_Right) { webkit_web_view_go_forward(app->web); }
        else if (!chrome->editing) { return FALSE; }
        else if (key == XKB_KEY_Escape) { focus_page(app); }
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
    if (type == WPE_EVENT_POINTER_DOWN && chrome->editing) { focus_page(app); }
    return FALSE;
}
static void chrome_clicked(struct browser_chrome *chrome, double x, double y, gpointer data) {
    struct app *app = data;
    int width = wpe_view_get_width(chrome->view);
    if (y < BROWSER_TAB_HEIGHT) {
        guint count = chrome->tabs->len;
        int add_x = browser_chrome_new_tab_x(chrome, width);
        if (x >= add_x && x < add_x + 32) { new_tab(app, "about:blank"); }
        else if (x >= width - 72) { select_tab(app, (chrome->active_tab + 1) % count); }
        else if (x >= width - 100) { select_tab(app, (chrome->active_tab + count - 1) % count); }
        else if (x >= 12) {
            guint slots = browser_chrome_tab_slots(width);
            guint slot = (guint)(x - 12) / BROWSER_TAB_WIDTH;
            guint index = chrome->active_tab / slots * slots + slot;
            if (slot < slots && index < count) {
                if (x - 12 - slot * BROWSER_TAB_WIDTH >= BROWSER_TAB_WIDTH - 32) { close_tab(app, index); }
                else { select_tab(app, index); }
            }
        }
    } else if (y < BROWSER_CHROME_HEIGHT) {
        if (x < 45) { webkit_web_view_go_back(app->web); }
        else if (x < 83) { webkit_web_view_go_forward(app->web); }
        else if (x < 127) { webkit_web_view_reload(app->web); }
        else if (x >= width - 112) { open_downloads(); }
        else { show_address(app); }
    }
}
static void changed_uri(WebKitWebView *web, GParamSpec *spec, gpointer data) {
    (void)spec; struct app *app = data;
    if (app->probe || web != app->web || app->chrome.editing) { return; }
    const char *uri = webkit_web_view_get_uri(web);
    g_strlcpy(app->chrome.address, uri && strcmp(uri, "about:blank") ? uri : "", sizeof(app->chrome.address));
    browser_chrome_update(&app->chrome);
}
static void changed_title(WebKitWebView *web, GParamSpec *spec, gpointer data) {
    (void)spec; struct app *app = data;
    if (app->probe) { return; }
    browser_chrome_update(&app->chrome);
    if (web == app->web) {
        const char *title = webkit_web_view_get_title(web);
        char *label = g_strdup_printf("%s — JLinux Browser", title && *title ? title : "Nova aba");
        wpe_toplevel_set_title(app->toplevel, label); g_free(label);
    }
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
    if (event == WEBKIT_LOAD_STARTED) {
        if (app->probe) { app->load_started = g_get_monotonic_time(); }
        g_object_set_data(G_OBJECT(web), "jlinux-notice", NULL);
    }
    if (event != WEBKIT_LOAD_FINISHED || app->status) { return; }
    if (app->probe) {
        webkit_web_view_evaluate_javascript(web,
            "document.readyState === 'complete' && !!document.body && (6 * 7 === 42)", -1, NULL, NULL, NULL, js_finished, app);
    } else { g_print("Página carregada\n"); }
}
static gboolean load_failed(WebKitWebView *web, WebKitLoadEvent event, const char *uri, GError *error, gpointer data) {
    (void)web; (void)event; (void)uri; struct app *app = data;
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)
        || g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)) { return TRUE; }
    g_printerr("Falha de carregamento: %s\n", error->message);
    if (app->probe) { app->status = 1; quit(app); }
    else { web_notice(app, web, "Não foi possível abrir a página. Confira o endereço e a conexão."); }
    return TRUE;
}
static gboolean policy(WebKitWebView *web, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data) {
    (void)web; struct app *app = data;
    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        webkit_policy_decision_ignore(decision);
        web_notice(app, web, "Janelas adicionais ainda não estão disponíveis."); return TRUE;
    }
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        const char *uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(action));
        // Blob URLs stay in the creating page's WebKit origin; address-bar input remains HTTP/HTTPS only.
        gboolean blob = uri && strlen(uri) <= BROWSER_ADDRESS_LIMIT
            && (g_str_has_prefix(uri, "blob:https://") || g_str_has_prefix(uri, "blob:http://"))
            && browser_uri_allowed(uri + 5);
        if (!browser_uri_allowed(uri) && !blob) {
            webkit_policy_decision_ignore(decision);
            web_notice(app, web, "Este endereço não é permitido nesta versão experimental.");
            if (app->probe) { app->status = 1; quit(app); } return TRUE;
        }
    }
    if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE
        && !webkit_response_policy_decision_is_mime_type_supported(WEBKIT_RESPONSE_POLICY_DECISION(decision))) {
        if (!app->probe && webkit_response_policy_decision_is_main_frame_main_resource(WEBKIT_RESPONSE_POLICY_DECISION(decision))) {
            webkit_policy_decision_download(decision);
        } else {
            webkit_policy_decision_ignore(decision);
            if (app->probe) { app->status = 1; quit(app); }
        }
        return TRUE;
    }
    return FALSE;
}
static gboolean permission(WebKitWebView *web, WebKitPermissionRequest *request, gpointer data) {
    struct app *app = data; webkit_permission_request_deny(request);
    web_notice(app, web, "Permissão recusada: controles de câmera, microfone e localização ainda indisponíveis."); return TRUE;
}
static gboolean authenticate(WebKitWebView *web, WebKitAuthenticationRequest *request, gpointer data) {
    struct app *app = data; webkit_authentication_request_cancel(request);
    web_notice(app, web, "Autenticação HTTP ainda não disponível nesta versão."); return TRUE;
}
static char *download_directory(void) {
    const char *configured = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
    return configured ? g_strdup(configured) : g_build_filename(g_get_home_dir(), "Downloads", NULL);
}
static void folder_opened(GObject *source, GAsyncResult *result, gpointer data) {
    (void)source; (void)data; GError *error = NULL;
    if (!g_app_info_launch_default_for_uri_finish(result, &error)) {
        g_printerr("Não foi possível abrir Downloads: %s\n", error->message); g_error_free(error);
    }
}
static void open_downloads(void) {
    char *directory = download_directory();
    if (g_mkdir_with_parents(directory, 0700) == 0) {
        char *uri = g_filename_to_uri(directory, NULL, NULL);
        if (uri) { g_app_info_launch_default_for_uri_async(uri, NULL, NULL, folder_opened, NULL); g_free(uri); }
    } else { g_printerr("Não foi possível criar a pasta Downloads.\n"); }
    g_free(directory);
}
static void download_progress(WebKitDownload *item, GParamSpec *spec, gpointer data) {
    (void)item; (void)spec; struct app *app = data;
    double total = 0;
    for (guint i = 0; i < app->downloads->len; i++) {
        total += webkit_download_get_estimated_progress(g_ptr_array_index(app->downloads, i));
    }
    g_snprintf(app->chrome.download_status, sizeof(app->chrome.download_status), "↓ %u · %.0f%%",
        app->downloads->len, app->downloads->len ? total * 100 / app->downloads->len : 0);
    if (!app->probe) { browser_chrome_update(&app->chrome); }
}
static gboolean download_destination(WebKitDownload *item, const char *suggested, gpointer data) {
    struct app *app = data; char *directory = download_directory();
    char *name = g_path_get_basename(suggested && *suggested ? suggested : "download");
    if (!strcmp(name, ".") || !strcmp(name, "..") || !strcmp(name, "/")) { g_free(name); name = g_strdup("download"); }
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_printerr("Download: não foi possível criar a pasta de destino.\n");
        webkit_download_cancel(item); g_free(name); g_free(directory); return TRUE;
    }
    char *path = NULL;
    for (guint suffix = 0; suffix < 10000; suffix++) {
        char *filename = suffix ? g_strdup_printf("%u-%s", suffix, name) : g_strdup(name);
        g_free(path); path = g_build_filename(directory, filename, NULL); g_free(filename);
        gboolean occupied = g_file_test(path, G_FILE_TEST_EXISTS) || g_file_test(path, G_FILE_TEST_IS_SYMLINK);
        for (guint i = 0; !occupied && i < app->downloads->len; i++) {
            WebKitDownload *other = g_ptr_array_index(app->downloads, i);
            occupied = other != item && !g_strcmp0(path, webkit_download_get_destination(other));
        }
        if (occupied) { continue; }
        webkit_download_set_allow_overwrite(item, FALSE);
        webkit_download_set_destination(item, path);
        g_free(path); g_free(name); g_free(directory); return TRUE;
    }
    g_printerr("Download: não foi possível escolher um nome livre.\n");
    webkit_download_cancel(item); g_free(path); g_free(name); g_free(directory); return TRUE;
}
static void download_failed(WebKitDownload *item, GError *error, gpointer data) {
    (void)data; g_object_set_data(G_OBJECT(item), "jlinux-download-failed", GINT_TO_POINTER(1));
    g_printerr("Download falhou: %s\n", error->message);
}
static void download_finished(WebKitDownload *item, gpointer data) {
    struct app *app = data;
    gboolean failed = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "jlinux-download-failed"));
    if (!failed) { g_print("Download concluído: %s\n", webkit_download_get_destination(item)); }
    g_signal_handlers_disconnect_by_data(item, app);
    g_ptr_array_remove(app->downloads, item);
    if (app->downloads->len) { download_progress(NULL, NULL, app); }
    else {
        g_strlcpy(app->chrome.download_status, failed ? "↓ !" : "↓ ✓", sizeof(app->chrome.download_status));
        if (!app->probe) { browser_chrome_update(&app->chrome); }
    }
}
static void download(WebKitNetworkSession *session, WebKitDownload *item, gpointer data) {
    (void)session; struct app *app = data;
    g_ptr_array_add(app->downloads, g_object_ref(item));
    g_signal_connect(item, "decide-destination", G_CALLBACK(download_destination), app);
    g_signal_connect(item, "notify::estimated-progress", G_CALLBACK(download_progress), app);
    g_signal_connect(item, "failed", G_CALLBACK(download_failed), app);
    g_signal_connect(item, "finished", G_CALLBACK(download_finished), app);
    download_progress(item, NULL, app);
}
static void terminated(WebKitWebView *web, WebKitWebProcessTerminationReason reason, gpointer data) {
    (void)web; struct app *app = data; g_printerr("Processo web encerrado: %d\n", reason);
    if (app->probe) { app->status = 1; quit(app); }
    else { web_notice(app, web, "A página parou. Use Ctrl+R para tentar novamente."); }
}
static void closed(WPEView *view, gpointer data) { (void)view; quit(data); }
static void disconnected(WPEDisplay *display, GError *error, gpointer data) {
    (void)display; struct app *app = data; app->status = 1;
    g_printerr("Conexão gráfica encerrada: %s\n", error ? error->message : "sem detalhes"); quit(app);
}
static void view_size_changed(WPEView *view, GParamSpec *spec, gpointer data) {
    (void)spec; struct app *app = data;
    WPEToplevel *toplevel = wpe_view_get_toplevel(view);
    if (!toplevel) { return; }
    int width, height; wpe_toplevel_get_size(toplevel, &width, &height);
    height = MAX(1, height - BROWSER_CHROME_HEIGHT);
    if (width > 0 && (wpe_view_get_width(view) != width || wpe_view_get_height(view) != height)) {
        wpe_view_resized(view, width, height);
        return;
    }
    if (view == app->chrome.view) { browser_chrome_update(&app->chrome); }
}
static gboolean timeout(gpointer data) {
    struct app *app = data; g_printerr("Tempo excedido no teste do motor web\n"); app->status = 1; quit(app); return G_SOURCE_CONTINUE;
}
static void configure_audio(void) {
    if (g_getenv("PULSE_SERVER") || g_getenv("PULSE_RUNTIME_PATH")) { return; }
    char *path = g_strdup_printf("/run/user/%lu/pulse/native", (unsigned long)getuid());
    struct stat metadata;
    // JLinux isolates Wayland in a private runtime; audio stays on the user's standard socket.
    if (!stat(path, &metadata) && S_ISSOCK(metadata.st_mode) && metadata.st_uid == getuid()) {
        char *server = g_strconcat("unix:", path, NULL);
        g_setenv("PULSE_SERVER", server, FALSE); g_free(server);
    }
    g_free(path);
}
static void select_tab(struct app *app, guint index) {
    if (index >= app->chrome.tabs->len) { return; }
    WebKitWebView *web = g_ptr_array_index(app->chrome.tabs, index);
    if (app->web && app->web != web) { wpe_view_set_visible(webkit_web_view_get_wpe_view(app->web), FALSE); }
    app->web = web; app->chrome.active_tab = index;
    app->chrome.view = webkit_web_view_get_wpe_view(web);
    app->chrome.adblock_enabled = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(web), "jlinux-adblock"));
    wpe_view_set_visible(app->chrome.view, TRUE);
    show_address(app); changed_title(web, NULL, app);
    const char *message = g_object_get_data(G_OBJECT(web), "jlinux-notice");
    if (message) { notice(app, message); }
}
static void new_tab(struct app *app, const char *uri) {
    WebKitSettings *settings = webkit_settings_new();
    webkit_settings_set_enable_media(settings, app->media_available);
    webkit_settings_set_enable_webaudio(settings, app->media_available);
    WebKitWebsitePolicies *policies = webkit_website_policies_new_with_policies("autoplay", WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND, NULL);
    WebKitWebView *web = g_object_new(WEBKIT_TYPE_WEB_VIEW, "display", app->display, "network-session", app->session,
        "settings", settings, "website-policies", policies, NULL);
    g_object_unref(settings); g_object_unref(policies);
    if (app->filter) {
        webkit_user_content_manager_add_filter(webkit_web_view_get_user_content_manager(web), app->filter);
        g_object_set_data(G_OBJECT(web), "jlinux-adblock", GINT_TO_POINTER(TRUE));
    }
    WPEView *view = webkit_web_view_get_wpe_view(web);
    g_ptr_array_add(app->chrome.tabs, web);
    g_signal_connect(view, "closed", G_CALLBACK(closed), app);
    g_signal_connect(web, "notify::title", G_CALLBACK(changed_title), app);
    g_signal_connect(web, "notify::uri", G_CALLBACK(changed_uri), app);
    g_signal_connect(web, "load-changed", G_CALLBACK(load_changed), app);
    g_signal_connect(web, "load-failed", G_CALLBACK(load_failed), app);
    g_signal_connect(web, "decide-policy", G_CALLBACK(policy), app);
    g_signal_connect(web, "permission-request", G_CALLBACK(permission), app);
    g_signal_connect(web, "authenticate", G_CALLBACK(authenticate), app);
    g_signal_connect(web, "web-process-terminated", G_CALLBACK(terminated), app);
    if (!app->probe) {
        wpe_view_set_visible(view, FALSE);
        // Clamp before WPE emits resized to WebKit, avoiding an intermediate full-window viewport.
        g_signal_connect(view, "notify::width", G_CALLBACK(view_size_changed), app);
        g_signal_connect(view, "notify::height", G_CALLBACK(view_size_changed), app);
        wpe_view_set_toplevel(view, app->toplevel);
        view_size_changed(view, NULL, app);
        g_signal_connect(view, "event", G_CALLBACK(view_event), app);
        select_tab(app, app->chrome.tabs->len - 1);
    } else { app->web = web; }
    webkit_web_view_load_uri(web, uri);
}
static void close_tab(struct app *app, guint index) {
    GPtrArray *tabs = app->chrome.tabs;
    if (index >= tabs->len) { return; }
    if (tabs->len == 1) { quit(app); return; }
    guint active = app->chrome.active_tab;
    WebKitWebView *web = g_ptr_array_steal_index(tabs, index);
    WPEView *view = webkit_web_view_get_wpe_view(web);
    g_signal_handlers_disconnect_by_data(web, app);
    g_signal_handlers_disconnect_by_data(view, app);
    wpe_view_set_visible(view, FALSE);
    wpe_view_set_toplevel(view, NULL);
    if (index == active) {
        app->web = NULL;
        select_tab(app, MIN(index, tabs->len - 1));
    } else {
        app->chrome.active_tab -= index < active;
        browser_chrome_update(&app->chrome);
    }
    g_object_unref(web);
}
static WebKitNetworkSession *network_session(gboolean probe) {
    if (probe) { return webkit_network_session_new_ephemeral(); }
    char *data = g_build_filename(g_get_user_data_dir(), "jlinux", "browser", NULL);
    char *cache = g_build_filename(g_get_user_cache_dir(), "jlinux", "browser", "webkit", NULL);
    const char *directories[] = {data, cache};
    for (guint i = 0; i < G_N_ELEMENTS(directories); i++) {
        struct stat st;
        if (g_mkdir_with_parents(directories[i], 0700) || lstat(directories[i], &st)
            || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || chmod(directories[i], 0700)) {
            g_printerr("Não foi possível preparar o perfil privado do navegador.\n");
            g_free(data); g_free(cache); return NULL;
        }
    }
    WebKitNetworkSession *session = webkit_network_session_new(data, cache);
    char *cookies = g_build_filename(data, "cookies.sqlite", NULL);
    webkit_cookie_manager_set_persistent_storage(webkit_network_session_get_cookie_manager(session),
        cookies, WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    g_free(cookies); g_free(data); g_free(cache); return session;
}
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        g_print("Uso: jlinux-browser [URL ou busca] | --probe URL | --preview arquivo.png\nCtrl+J Downloads; Ctrl+L endereço; Ctrl+T nova aba; Ctrl+W fechar aba; Ctrl+Tab alternar; Ctrl+Shift+B alternar bloqueio de anúncios nesta aba; Ctrl+Q sair.\n"); return 0;
    }
    if (g_getenv("WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS")) {
        g_printerr("Remova WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS: a sandbox é obrigatória.\n"); return 1;
    }
    struct app app = {.started = g_get_monotonic_time()};
    wl_list_init(&app.chrome.buffers);
    if (!jlinux_glass_settings(&app.chrome.glass)) { return 1; }
    if (argc == 3 && !strcmp(argv[1], "--preview")) {
        app.chrome.adblock_available = TRUE; app.chrome.adblock_enabled = TRUE;
        g_strlcpy(app.chrome.address, "https://example.org", sizeof(app.chrome.address));
        cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1000, BROWSER_CHROME_HEIGHT);
        cairo_t *cr = cairo_create(image); browser_chrome_paint(&app.chrome, cr, 1000); cairo_destroy(cr);
        cairo_status_t result = cairo_surface_write_to_png(image, argv[2]); cairo_surface_destroy(image);
        ui_finish(); return result == CAIRO_STATUS_SUCCESS ? 0 : 1;
    }
    app.probe = argc == 3 && !strcmp(argv[1], "--probe");
    if (argc > 2 && !app.probe) {
        g_printerr("Uso: jlinux-browser [URL ou busca] | --probe URL | --preview arquivo.png\n"); return 2;
    }
    char *uri = browser_address_uri(app.probe ? argv[2] : argc == 2 ? argv[1] : "about:blank");
    if (!uri) { g_printerr("Endereço inválido: use HTTP ou HTTPS.\n"); return 2; }
    configure_audio();
    if (gst_init_check(NULL, NULL, NULL)) {
        GstElementFactory *sink = gst_element_factory_find("autoaudiosink");
        GstElementFactory *player = gst_element_factory_find("playbin3");
        app.media_available = sink && player;
        if (sink) { gst_object_unref(sink); }
        if (player) { gst_object_unref(player); }
    }
    if (!app.media_available) { g_printerr("Plugins GStreamer ausentes; mídia desativada para evitar abort no WPE.\n"); }
    // Never fall back to DRM: the browser must not acquire a VT or replace the desktop.
    WPEDisplay *display = app.probe ? wpe_display_headless_new() : wpe_display_wayland_new();
    GError *error = NULL;
    if (!wpe_display_connect(display, &error)) {
        g_printerr("Não foi possível conectar ao display: %s\n", error->message);
        g_clear_error(&error); g_object_unref(display); g_free(uri); return 1;
    }
    app.loop = g_main_loop_new(NULL, FALSE);
    app.display = display;
    WebKitNetworkSession *session = network_session(app.probe);
    if (!session) { g_main_loop_unref(app.loop); g_object_unref(display); g_free(uri); return 1; }
    app.session = session;
    webkit_network_session_set_tls_errors_policy(session, WEBKIT_TLS_ERRORS_POLICY_FAIL);
    app.chrome.tabs = g_ptr_array_new_with_free_func(g_object_unref);
    const char *blocker = g_getenv("JLINUX_BROWSER_ADBLOCK");
    if (!blocker || strcmp(blocker, "0")) {
        app.filter = browser_blocker_load(&error);
        if (!app.filter) {
            g_printerr("Bloqueio de anúncios indisponível: %s\n", error ? error->message : "sem detalhes");
            g_clear_error(&error);
        }
    }
    app.chrome.adblock_available = app.filter != NULL;
    if (!app.probe) {
        if (!wpe_settings_set_boolean(wpe_display_get_settings(display), WPE_SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL,
                FALSE, WPE_SETTINGS_SOURCE_APPLICATION, &error)) {
            g_printerr("Não foi possível configurar abas: %s\n", error->message);
            g_clear_error(&error); app.status = 1; goto cleanup;
        }
        app.toplevel = wpe_display_create_toplevel(display, 0);
        if (!app.toplevel) { app.status = 1; goto cleanup; }
    }
    new_tab(&app, uri);
    WPEView *view = webkit_web_view_get_wpe_view(app.web);
    g_signal_connect(display, "disconnected", G_CALLBACK(disconnected), &app);
    app.downloads = g_ptr_array_new_with_free_func(g_object_unref);
    g_signal_connect(session, "download-started", G_CALLBACK(download), &app);
    if (!app.probe) {
        app.chrome.clicked = chrome_clicked; app.chrome.clicked_data = &app;
        if (!browser_chrome_init(&app.chrome, view)) {
            g_printerr("Não foi possível criar a barra nativa Wayland.\n"); app.status = 1; goto cleanup;
        }
        wpe_toplevel_set_title(wpe_view_get_toplevel(view), "JLinux Browser");
        show_address(&app);
        if (strcmp(uri, "about:blank")) { g_strlcpy(app.chrome.address, uri, sizeof(app.chrome.address)); }
    }
    guint term = g_unix_signal_add(SIGTERM, quit, &app), interrupt = g_unix_signal_add(SIGINT, quit, &app);
    guint deadline = app.probe ? g_timeout_add_seconds(20, timeout, &app) : 0;
    g_main_loop_run(app.loop);
    if (deadline) { g_source_remove(deadline); }
    g_source_remove(term); g_source_remove(interrupt);
cleanup:
    if (app.downloads) {
        for (guint i = 0; i < app.downloads->len; i++) {
            WebKitDownload *item = g_ptr_array_index(app.downloads, i);
            g_signal_handlers_disconnect_by_data(item, &app); webkit_download_cancel(item);
        }
        g_ptr_array_unref(app.downloads);
    }
    for (guint i = 0; i < app.chrome.tabs->len; i++) {
        WebKitWebView *web = g_ptr_array_index(app.chrome.tabs, i);
        g_signal_handlers_disconnect_by_data(webkit_web_view_get_wpe_view(web), &app);
        g_signal_handlers_disconnect_by_data(web, &app);
    }
    g_signal_handlers_disconnect_by_data(session, &app);
    g_signal_handlers_disconnect_by_data(display, &app);
    browser_chrome_finish(&app.chrome);
    g_ptr_array_unref(app.chrome.tabs);
    if (app.filter) { webkit_user_content_filter_unref(app.filter); }
    g_clear_object(&app.toplevel);
    g_object_unref(session);
    // shortcut: WPE 2.54 disconnects Wayland before EGL teardown; remove when upstream fixes disposal order.
    if (!app.probe) {
        EGLDisplay egl = wpe_display_get_egl_display(display, NULL);
        if (egl != EGL_NO_DISPLAY) { eglTerminate(egl); }
    }
    g_object_unref(display);
    g_main_loop_unref(app.loop); g_free(uri);
    return app.status;
}
