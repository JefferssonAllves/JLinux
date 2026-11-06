#define _GNU_SOURCE
#include "../toolkit/painter.h"
#include "volume.h"
#include <glib.h>
#include "futurista.h"
#include "control.h"
#include <sys/socket.h>
#include "../effects/glass.h"
#include "../renderer/blur.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "layer-shell-client.h"
extern char **environ;
enum { PANEL_BODY_HEIGHT = 52, PANEL_FADE_HEIGHT = 0,
    PANEL_HEIGHT = PANEL_BODY_HEIGHT + PANEL_FADE_HEIGHT, PANEL_MARGIN = 6, PANEL_ZONE = PANEL_HEIGHT };
struct app;
struct surface {
    struct app *app;
    struct wl_surface *wl;
    struct zwlr_layer_surface_v1 *layer;
    const char *name;
    int width, height, buffers;
    bool dirty, configured;
};
struct buffer {
    struct surface *surface;
    struct wl_buffer *wl;
    void *data;
    size_t size;
    struct wl_list link;
};
struct app {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layers;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
    struct surface wallpaper, panel, power, audio, launcher;
    struct futurista futurista;
    bool showing_controls;
    int control;
    struct volume volume;
    struct surface *pointer_surface;
    struct wl_list buffers;
    double pointer_x, pointer_y;
    bool running, info, power_panel, volume_panel;
    int power_action;
    pid_t power_pid;
    bool power_error;
    int timer, signals;
    char notice[180];
    struct jlinux_glass glass;
};
static const char *power_labels[] = {"Sair da sessão", "Reiniciar", "Desligar"};
static void paint_power(struct app *app, cairo_t *cr) {
    cairo_set_source_rgba(cr, .025, .035, .075, .96); cairo_paint(cr);
    jlinux_glass_paint(cr, (struct ui_rect){1, 1, 318, 258}, 12, NULL, 0, 0, app->glass);
    ui_text(cr, "Sessão e energia", 18, 14, 17, true, 0xf1f2fc);
    if (app->power_pid) {
        ui_text(cr, "Solicitando ao sistema…", 18, 65, 14, false, 0xf1f2fc);
    } else if (app->power_action >= 0) {
        ui_text(cr, power_labels[app->power_action], 18, 60, 17, true, 0xf1f2fc);
        ui_text(cr, "Os aplicativos serão encerrados.", 18, 96, 14, false, 0xd2d7eb);
        ui_text(cr, "Salve seu trabalho antes de confirmar.", 18, 122, 14, false, 0xd2d7eb);
        ui_text(cr, "Confirmar", 18, 168, 16, true, 0xf1f2fc);
    } else {
        for (int i = 0; i < 3; i++) {
            ui_text(cr, power_labels[i], 18, 55 + i * 42, 16, false, 0xf1f2fc);
        }
    }
    if (app->power_error) {
        ui_text(cr, "Ação recusada ou indisponível.", 18, 180, 13, true, 0xffb8bd);
        ui_text(cr, "Veja o log da sessão para detalhes.", 18, 200, 13, false, 0xd2d7eb);
    }
    ui_text(cr, app->power_pid ? "Fechar" : "Cancelar / Fechar", 18, 230, 14, false, 0xa3aec9);
}
static void paint_volume(struct app *app, cairo_t *cr) {
    cairo_set_source_rgb(cr, .025, .035, .075); cairo_paint(cr);
    jlinux_glass_paint(cr, (struct ui_rect){1, 1, 298, 158}, 10, NULL, 0, 0, app->glass);
    ui_text(cr, "Volume", 18, 14, 17, true, 0xf1f2fc);
    char label[64];
    if (app->volume.pid) { snprintf(label, sizeof(label), "Atualizando…"); }
    else if (!app->volume.available) { snprintf(label, sizeof(label), "Áudio indisponível · tentar novamente"); }
    else { snprintf(label, sizeof(label), "%.0f%%%s", app->volume.level * 100, app->volume.muted ? " · silenciado" : ""); }
    ui_text(cr, label, 18, 48, 13, false, 0xd2d7eb);
    ui_text(cr, "−", 24, 82, 22, true, 0xf1f2fc);
    ui_text(cr, app->volume.muted ? "Ativar som" : "Silenciar", 80, 88, 14, true, 0xf1f2fc);
    ui_text(cr, "+", 250, 82, 22, true, 0xf1f2fc);
    ui_text(cr, "Fechar", 18, 132, 13, false, 0xa3aec9);
}
static void paint(struct surface *surface, cairo_t *cr) {
    int width = surface->width, height = surface->height;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_set_source_rgba(cr, 0, 0, 0, 0); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    if (surface == &surface->app->launcher) {
        surface->app->futurista.volume = surface->app->volume.available ? surface->app->volume.level : -1;
        surface->app->futurista.muted = surface->app->volume.muted;
        futurista_overlay(&surface->app->futurista, cr, width, height, surface->app->showing_controls);
    } else if (surface == &surface->app->audio) {
        paint_volume(surface->app, cr);
    } else if (surface == &surface->app->panel && surface->app->volume_panel) {
        ui_text(cr, "Som", 10, 8, 13, true, 0xe9eafa);

    } else if (surface == &surface->app->power) {
        paint_power(surface->app, cr);
    } else if (surface == &surface->app->panel && surface->app->power_panel) {
        ui_icon(cr, UI_POWER, 0, 6, 23, 0xe9eafa);
    } else if (surface == &surface->app->wallpaper) {
        futurista_wallpaper(&surface->app->futurista, cr, width, height);
    } else {
        futurista_panel(&surface->app->futurista, cr, width);
        if (surface->app->info) {
            /* shortcut: show help in the existing panel; real popovers arrive with shell navigation. */
            cairo_set_source_rgb(cr, .035, .045, .09); cairo_rectangle(cr, 8, 10, width - 16, 34); cairo_fill(cr);
            ui_text(cr, surface->app->notice, 24, 17, 14, false, 0xe6defe);
        }
    }
}
static void buffer_destroy(struct buffer *buffer) {
    wl_buffer_destroy(buffer->wl); munmap(buffer->data, buffer->size);
    wl_list_remove(&buffer->link); buffer->surface->buffers--; free(buffer);
}
static void released(void *data, struct wl_buffer *wl) { (void)wl; buffer_destroy(data); }
static const struct wl_buffer_listener buffer_listener = {.release = released};
static bool draw(struct surface *surface) {
    if (!surface->configured || !surface->dirty || surface->buffers >= 2) { return true; }
    if (surface->width < 1 || surface->height < 1 || surface->width > 8192 || surface->height > 8192) { return false; }
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, surface->width);
    if (stride < 0 || surface->height > INT_MAX / stride) { return false; }
    size_t size = (size_t)stride * surface->height;
    struct buffer *buffer = calloc(1, sizeof(*buffer));
    if (!buffer) { return false; }
    int fd = memfd_create("jlinux-shell", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, (off_t)size) < 0) {
        if (fd >= 0) { close(fd); } free(buffer); return false;
    }
    void *mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { close(fd); free(buffer); return false; }
    struct wl_shm_pool *pool = wl_shm_create_pool(surface->app->shm, fd, (int)size);
    if (!pool) { close(fd); munmap(mapping, size); free(buffer); return false; }
    buffer->wl = wl_shm_pool_create_buffer(pool, 0, surface->width, surface->height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool); close(fd);
    if (!buffer->wl) { munmap(mapping, size); free(buffer); return false; }
    buffer->surface = surface; buffer->data = mapping; buffer->size = size;
    wl_list_insert(&surface->app->buffers, &buffer->link); surface->buffers++;
    wl_buffer_add_listener(buffer->wl, &buffer_listener, buffer);
    cairo_surface_t *image = cairo_image_surface_create_for_data(mapping, CAIRO_FORMAT_ARGB32,
        surface->width, surface->height, stride);
    cairo_t *cr = cairo_create(image); paint(surface, cr);
    bool valid = cairo_status(cr) == CAIRO_STATUS_SUCCESS && cairo_surface_status(image) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr); cairo_surface_destroy(image);
    if (!valid) { buffer_destroy(buffer); return false; }
    wl_surface_attach(surface->wl, buffer->wl, 0, 0);
    wl_surface_damage_buffer(surface->wl, 0, 0, surface->width, surface->height);
    wl_surface_commit(surface->wl); surface->dirty = false;
    return true;
}
static void configure(void *data, struct zwlr_layer_surface_v1 *layer, uint32_t serial, uint32_t width, uint32_t height) {
    struct surface *surface = data;
    zwlr_layer_surface_v1_ack_configure(layer, serial);
    if (width > 8192 || height > 8192) {
        fprintf(stderr, "Configuração inválida de %s: %ux%u\n", surface->name, width, height);
        surface->app->running = false; return;
    }
    /* Keep the last usable buffer while an output has no drawable extent. */
    if (!width || !height) { return; }
    surface->width = (int)width; surface->height = (int)height;
    surface->configured = true; surface->dirty = true;
}
static void closed(void *data, struct zwlr_layer_surface_v1 *layer) {
    (void)layer; struct surface *surface = data;
    fprintf(stderr, "Compositor fechou a superfície %s.\n", surface->name); surface->app->running = false;
}
static const struct zwlr_layer_surface_v1_listener layer_listener = {.configure = configure, .closed = closed};
static bool create_surface(struct app *app, struct surface *surface, const char *name,
                           uint32_t level, uint32_t anchor, int width, int height, int zone, int margin) {
    surface->app = app; surface->name = name;
    surface->wl = wl_compositor_create_surface(app->compositor);
    if (!surface->wl) { return false; }
    surface->layer = zwlr_layer_shell_v1_get_layer_surface(app->layers, surface->wl, NULL, level, name);
    if (!surface->layer) { return false; }
    zwlr_layer_surface_v1_add_listener(surface->layer, &layer_listener, surface);
    zwlr_layer_surface_v1_set_size(surface->layer, width, height);
    zwlr_layer_surface_v1_set_anchor(surface->layer, anchor);
    zwlr_layer_surface_v1_set_exclusive_zone(surface->layer, zone);
    zwlr_layer_surface_v1_set_margin(surface->layer, margin, margin, margin, margin);
    zwlr_layer_surface_v1_set_keyboard_interactivity(surface->layer, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    if (surface == &app->panel && !app->power_panel && !app->volume_panel) {
        struct wl_region *input = wl_compositor_create_region(app->compositor);
        if (!input) { return false; }
        wl_region_add(input, 0, 0, 8192, PANEL_BODY_HEIGHT);
        wl_surface_set_input_region(surface->wl, input); wl_region_destroy(input);
    }
    if(surface==&app->launcher){
        struct wl_region *input=wl_compositor_create_region(app->compositor);
        if(!input){return false;}
        wl_region_add(input,0,PANEL_BODY_HEIGHT,8192,8192);
        wl_surface_set_input_region(surface->wl,input);wl_region_destroy(input);
    }
    if (surface == &app->wallpaper) {
        struct wl_region *empty = wl_compositor_create_region(app->compositor);
        if (!empty) { return false; }
        wl_surface_set_input_region(surface->wl, empty); wl_region_destroy(empty);
    }
    wl_surface_commit(surface->wl);
    return true;
}
static void notice(struct app *app, const char *message) {
    snprintf(app->notice, sizeof(app->notice), "%s", message); app->info = true; app->panel.dirty = true;
}
static void toggle_volume(struct app *app) {
    if (app->audio.wl) {
        if (app->pointer_surface == &app->audio) { app->pointer_surface = NULL; }
        zwlr_layer_surface_v1_destroy(app->audio.layer); wl_surface_destroy(app->audio.wl);
        app->audio.layer = NULL; app->audio.wl = NULL; app->audio.configured = false; app->audio.dirty = false;
        return;
    }
    if (!create_surface(app, &app->audio, "jlinux-volume", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT, 300, 160, -1, PANEL_MARGIN)) {
        app->running = false; return;
    }
    zwlr_layer_surface_v1_set_margin(app->audio.layer, 48, 60, 0, 0);
    wl_surface_commit(app->audio.wl);
    volume_start(&app->volume, 0);
}
static void close_power(struct app *app) {
    if (app->pointer_surface == &app->power) { app->pointer_surface = NULL; }
    if (app->power.layer) { zwlr_layer_surface_v1_destroy(app->power.layer); }
    if (app->power.wl) { wl_surface_destroy(app->power.wl); }
    app->power.layer = NULL; app->power.wl = NULL;
    app->power.configured = false; app->power.dirty = false;
    app->power_action = -1;
}
static void toggle_power(struct app *app) {
    if (app->power.wl) { close_power(app); return; }
    app->power_action = -1; app->power_error = false;
    if (!create_surface(app, &app->power, "jlinux-power-menu", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT, 320, 260, -1, PANEL_MARGIN)) {
        close_power(app); notice(app, "Não foi possível abrir o menu de energia."); return;
    }
    zwlr_layer_surface_v1_set_margin(app->power.layer, PANEL_MARGIN + PANEL_BODY_HEIGHT + 6, PANEL_MARGIN, 0, 0);
    wl_surface_commit(app->power.wl);
}
static void request_power(struct app *app) {
    if (app->power_pid || app->power_action < 0 || app->power_action > 2) { return; }
    char *session = getenv("XDG_SESSION_ID");
    if (app->power_action == 0 && (!session || !*session ||
        strspn(session, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(session))) {
        app->power_error = true;
        app->power_action = -1; app->power.dirty = true;
        fprintf(stderr, "Logout recusado: XDG_SESSION_ID ausente ou inválido.\n"); return;
    }
    char *logout[] = {"loginctl", "--no-ask-password", "terminate-session", session, NULL};
    char *power[] = {"systemctl", "--no-ask-password", "--check-inhibitors=yes",
        app->power_action == 1 ? "reboot" : "poweroff", NULL};
    char **args = app->power_action == 0 ? logout : power;
    posix_spawnattr_t attributes;
    int error = posix_spawnattr_init(&attributes);
    if (!error) {
        sigset_t empty; sigemptyset(&empty);
        error = posix_spawnattr_setsigmask(&attributes, &empty);
        if (!error) { error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK); }
        if (!error) { error = posix_spawnp(&app->power_pid, args[0], NULL, &attributes, args, environ); }
        posix_spawnattr_destroy(&attributes);
    }
    if (error) {
        app->power_pid = 0; app->power_action = -1;
        app->power_error = true;
        fprintf(stderr, "Ação de sessão falhou: %s\n", strerror(error));
    }
    app->power.dirty = true;
}
static void close_launcher(struct app *app) {
    if (!app->launcher.wl) {
        return;
    }
    if (app->pointer_surface == &app->launcher) {
        app->pointer_surface = NULL;
    }
    zwlr_layer_surface_v1_destroy(app->launcher.layer);
    wl_surface_destroy(app->launcher.wl);
    app->launcher.layer = NULL;
    app->launcher.wl = NULL;
    app->launcher.configured = false;
    app->launcher.dirty = false;
    fprintf(stderr, "%s\n",app->showing_controls?"Painel de controle fechado.":"Dock circular fechado.");
}
static void toggle_launcher(struct app *app, bool controls) {
    if(app->launcher.wl){
        bool same=app->showing_controls==controls;close_launcher(app);
        if(same){return;}
    }
    app->showing_controls=controls;
    uint32_t all = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    if (!create_surface(app, &app->launcher, controls?"jlinux-control-center":"jlinux-radial-dock", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, all, 0,
                        0, -1, 0)) {
        app->running = false;
        return;
    }
    futurista_status(&app->futurista);
    if(controls){volume_start(&app->volume, 0);}
    app->futurista.message[0] = 0;
    fprintf(stderr,"%s\n",controls?"Painel de controle aberto.":"Dock circular aberto.");
}
static void reset_child_signals(gpointer data) {
    (void)data;
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);
}
static void launch_item(struct app *app, int index) {
    static const char *commands[] = {"firefox", "foot",         "code",         "spotify", "jlinux-files",
                                     NULL,      "jlinux-files", "jlinux-files", "foot",    "steam"};
    if (index < 0 || index >= 10) {
        return;
    }
    if (index == 5) {
        snprintf(app->futurista.message, sizeof(app->futurista.message),
                 "Abra o painel pelo botão à direita da barra.");
        app->launcher.dirty = true;
        return;
    }
    const char *command = commands[index];
    /* Keep the reference icon; use our browser when Firefox is absent. */
    char *found = g_find_program_in_path(command);
    if (!found && index == 0) {
        command = "jlinux-browser";
        found = g_find_program_in_path(command);
    }
    if (!found) {
        snprintf(app->futurista.message, sizeof(app->futurista.message), "Aplicativo não instalado: %s",
                 command);
        app->launcher.dirty = true;
        return;
    }
    char *folder = index == 6   ? g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_PICTURES))
                   : index == 7 ? g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS))
                                : NULL;
    char *files_args[] = {(char *)command, folder, NULL};
    char *terminal_args[] = {"foot", "--term=xterm-256color", "--title=JLinux Terminal", NULL};
    char **args = !strcmp(command, "foot") ? terminal_args : files_args;
    GError *error = NULL;
    if (!g_spawn_async(NULL, args, NULL, G_SPAWN_SEARCH_PATH, reset_child_signals, NULL, NULL, &error)) {
        snprintf(app->futurista.message, sizeof(app->futurista.message), "Falha ao abrir aplicativo: %s",
                 error->message);
        g_error_free(error);
        app->launcher.dirty = true;
    } else {
        close_launcher(app);
    }
    g_free(found);
    g_free(folder);
}
static void launcher_action(struct app *app, int hit) {
    if (hit >= 0 && hit < 10) {
        app->futurista.selected = hit;
        launch_item(app, hit);
        return;
    }
    if (hit == 10) {
        launch_item(app, app->futurista.selected);
        return;
    }
    if (hit == 29) {
        volume_start(&app->volume, 3);
        return;
    }
    if (hit == 30 || hit == 31) {
        volume_start(&app->volume, hit == 30 ? 1 : 2);
        return;
    }
    if (hit >= 33 && hit <= 35) {
        close_launcher(app);
        toggle_power(app);
        app->power_action = hit - 33;
        app->power.dirty = true;
        return;
    }
    if (hit == -1) {
        close_launcher(app);
        return;
    }
    snprintf(app->futurista.message, sizeof(app->futurista.message),
             "Este controle ainda não está disponível.");
    app->launcher.dirty = true;
}
static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *wl, wl_fixed_t x, wl_fixed_t y) {
    struct app *app = data;
    struct wl_cursor *cursor = app->cursor_theme ? wl_cursor_theme_get_cursor(app->cursor_theme, "left_ptr") : NULL;
    if (cursor && cursor->image_count) {
        struct wl_cursor_image *image = cursor->images[0];
        wl_pointer_set_cursor(pointer, serial, app->cursor_surface, (int)image->hotspot_x, (int)image->hotspot_y);
        wl_surface_attach(app->cursor_surface, wl_cursor_image_get_buffer(image), 0, 0);
        wl_surface_damage(app->cursor_surface, 0, 0, (int)image->width, (int)image->height);
        wl_surface_commit(app->cursor_surface);
    }
    struct surface *surfaces[] = {&app->panel, &app->power, &app->audio, &app->launcher};
    app->pointer_surface = NULL;
    for (size_t i = 0; i < 4; i++) { if (surfaces[i]->wl == wl) { app->pointer_surface = surfaces[i]; } }
    app->pointer_x = wl_fixed_to_double(x); app->pointer_y = wl_fixed_to_double(y);
}
static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *wl) {
    (void)pointer; (void)serial; (void)wl;
    struct app *app = data;
    app->pointer_surface = NULL;
}
static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)pointer; (void)time;
    struct app *app = data; app->pointer_x = wl_fixed_to_double(x); app->pointer_y = wl_fixed_to_double(y);
    if(app->pointer_surface==&app->launcher){
        int hit=futurista_hit(app->pointer_x,app->pointer_y,app->launcher.width,app->launcher.height,app->showing_controls);
        if(hit>=0 && hit<10 && hit!=app->futurista.selected){app->futurista.selected=hit;app->launcher.dirty=true;}
    }
}
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void)pointer; (void)serial; (void)time;
    struct app *app = data;
    if (button != BTN_LEFT || state != WL_POINTER_BUTTON_STATE_RELEASED) { return; }
    if (app->pointer_surface == &app->launcher) {
        launcher_action(app,futurista_hit(app->pointer_x,app->pointer_y,app->launcher.width,app->launcher.height,app->showing_controls));
    } else if (app->pointer_surface == &app->audio) {
        if (app->pointer_y >= 122) { toggle_volume(app); }
        else if (app->pointer_y >= 78 && app->pointer_y < 116) {
            volume_start(&app->volume, app->pointer_x < 64 ? 1 : app->pointer_x > 230 ? 2 : 3);
            app->audio.dirty = true;
        } else if (app->pointer_y >= 42) { volume_start(&app->volume, 0); app->audio.dirty = true; }
    } else if (app->pointer_surface == &app->power) {
        double y = app->pointer_y;
        if (app->pointer_x < 8 || app->pointer_x > 312) { return; }
        if (y >= 220) { close_power(app); }
        else if (!app->power_pid && app->power_action >= 0 && y >= 158 && y < 202) { request_power(app); }
        else if (!app->power_pid && app->power_action < 0 && y >= 48 && y < 174) {
            app->power_action = (int)(y - 48) / 42;
            app->power_error = false; app->power.dirty = true;
        }
    } else if (app->pointer_surface == &app->panel) {
        if (!app->volume_panel && !app->power_panel) {
            if(app->pointer_x>=app->panel.width-60){toggle_launcher(app,true);}
            else if(app->pointer_x>=app->panel.width-226 && app->pointer_x<app->panel.width-188){toggle_volume(app);}

        }
        else if (app->volume_panel || (app->pointer_x >= app->panel.width - 326 && app->pointer_x < app->panel.width - 272)) { toggle_volume(app); }
        else if (app->info) { app->info = false; app->panel.dirty = true; }
        else if (app->pointer_x > app->panel.width - 60) {
            toggle_power(app);
        } else if (app->pointer_x >= app->panel.width - 272) { notice(app, "Wi-Fi e Bluetooth ainda serão conectados aos serviços do sistema."); }
    }
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data; (void)pointer; (void)time; (void)axis; (void)value;
}
static const struct wl_pointer_listener pointer_listener = {.enter = pointer_enter, .leave = pointer_leave,
    .motion = pointer_motion, .button = pointer_button, .axis = pointer_axis};
static void capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
    struct app *app = data;
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {
        app->pointer = wl_seat_get_pointer(seat); wl_pointer_add_listener(app->pointer, &pointer_listener, app);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && app->pointer) {
        wl_pointer_destroy(app->pointer); app->pointer = NULL; app->pointer_surface = NULL;
    }
}
static const struct wl_seat_listener seat_listener = {.capabilities = capabilities};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct app *app = data;
    if (!strcmp(interface, wl_compositor_interface.name) && version >= 4) {
        app->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (!strcmp(interface, wl_shm_interface.name)) {
        app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name)) {
        app->layers = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (!strcmp(interface, wl_seat_interface.name) && !app->seat) {
        app->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1); wl_seat_add_listener(app->seat, &seat_listener, app);
    }
}
static void global_remove(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
static const struct wl_registry_listener registry_listener = {.global = global, .global_remove = global_remove};
static int preview(const char *path) {
    struct app app = {0};
    if (!jlinux_glass_settings(&app.glass)) { return 1; }
    if (!futurista_init(&app.futurista,app.glass)) { futurista_finish(&app.futurista); return 1; }
    app.wallpaper = (struct surface){.app = &app, .width = 1672, .height = 941};
    app.panel = (struct surface){.app = &app, .width = 1672, .height = PANEL_HEIGHT};
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1672, 941);
    cairo_t *cr = cairo_create(image); paint(&app.wallpaper, cr);
    cairo_push_group(cr); paint(&app.panel, cr);
    cairo_pop_group_to_source(cr); cairo_paint(cr); cairo_identity_matrix(cr);
    cairo_status_t status = cairo_surface_write_to_png(image, path);
    cairo_destroy(cr); cairo_surface_destroy(image);
    futurista_finish(&app.futurista);
    ui_finish();
    return status == CAIRO_STATUS_SUCCESS ? 0 : 1;
}
static int preview_futurista(const char *path, bool controls) {
    struct futurista scene;
    struct jlinux_glass effects;
    if (!jlinux_glass_settings(&effects)) { return 1; }
    if (!futurista_init(&scene,effects)) { futurista_finish(&scene); return 1; }
    scene.wifi = 1;
    scene.battery = 75;
    scene.volume = .40;
    scene.brightness = .53;
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1672, 941);
    cairo_t *cr = cairo_create(image);
    futurista_wallpaper(&scene, cr, 1672, 941);
    futurista_panel(&scene, cr, 1672);
    futurista_overlay(&scene, cr, 1672, 941, controls);
    cairo_status_t status = cairo_surface_write_to_png(image, path);
    cairo_destroy(cr);
    cairo_surface_destroy(image);
    futurista_finish(&scene);
    ui_finish();
    return status == CAIRO_STATUS_SUCCESS ? 0 : 1;
}
static int preview_power(const char *path, bool volume) {
    struct app app = {.power_action = -1};
    if (!jlinux_glass_settings(&app.glass)) { return 1; }
    if (!futurista_init(&app.futurista,app.glass)) { futurista_finish(&app.futurista); return 1; }
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, volume ? 300 : 320, volume ? 160 : 260);
    cairo_t *cr = cairo_create(image);
    if (volume) { app.volume.available = true; app.volume.level = .4; paint_volume(&app, cr); }
    else { paint_power(&app, cr); }
    cairo_status_t status = cairo_surface_write_to_png(image, path);
    cairo_destroy(cr); cairo_surface_destroy(image); futurista_finish(&app.futurista); ui_finish();
    return status == CAIRO_STATUS_SUCCESS ? 0 : 1;
}
int main(int argc, char **argv) {
    bool volume_panel = argc == 2 && !strcmp(argv[1], "--volume-panel");
    bool power_panel = argc == 2 && !strcmp(argv[1], "--power-panel");
    if (argc == 3 && !strcmp(argv[1], "--preview-volume")) { return preview_power(argv[2], true); }
    if (argc == 3 && !strcmp(argv[1], "--preview-power")) { return preview_power(argv[2], false); }
    if (argc == 3 && !strcmp(argv[1], "--preview-control-center")) { return preview_futurista(argv[2],true); }
    if (argc == 3 && !strcmp(argv[1], "--preview-futurista")) { return preview_futurista(argv[2],false); }
    if (argc == 3 && !strcmp(argv[1], "--preview")) { return preview(argv[2]); }
    if (argc != 1 && !power_panel && !volume_panel) { fprintf(stderr, "Uso: jlinux-shell [--preview arquivo.png | --preview-power arquivo.png | --power-panel | --volume-panel]\n"); return 1; }
    struct app app = {.running = true, .power_panel = power_panel, .volume_panel = volume_panel, .power_action = -1, .timer = -1, .signals = -1, .control = -1}; wl_list_init(&app.buffers);
    if (getenv("JLINUX_THEME")) { fprintf(stderr, "JLINUX_THEME/CSS não é suportado pelo shell nativo.\n"); return 1; }
    if (!jlinux_glass_settings(&app.glass)) { return 1; }
    if (!futurista_init(&app.futurista,app.glass)) { futurista_finish(&app.futurista); return 1; }
    sigset_t signals; sigemptyset(&signals); sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM); sigaddset(&signals, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &signals, NULL)) { return 1; }
    app.signals = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    app.timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    app.display = wl_display_connect(NULL);
    int result = 1;
    if (!app.display || app.signals < 0 || app.timer < 0) { goto cleanup; }
    if(!power_panel && !volume_panel){app.control=shell_control_open();if(app.control<0){goto cleanup;}}
    futurista_status(&app.futurista);
    app.registry = wl_display_get_registry(app.display); wl_registry_add_listener(app.registry, &registry_listener, &app);
    if (wl_display_roundtrip(app.display) < 0 || !app.compositor || !app.shm || !app.layers) { goto cleanup; }
    app.cursor_theme = wl_cursor_theme_load(NULL, 24, app.shm);
    app.cursor_surface = wl_compositor_create_surface(app.compositor);
    uint32_t all = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    if (!power_panel && !volume_panel && !create_surface(&app, &app.wallpaper, "jlinux-wallpaper", ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, all, 0, 0, -1, 0)) { goto cleanup; }
    if (!volume_panel && !create_surface(&app, &app.panel, power_panel ? "jlinux-power-button" : "jlinux-panel", power_panel ? ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY : ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
        (power_panel ? 0 : ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT), power_panel ? 44 : 0, power_panel ? PANEL_BODY_HEIGHT : PANEL_HEIGHT, power_panel ? -1 : PANEL_ZONE, power_panel ? PANEL_MARGIN : 0)) { goto cleanup; }
    if (volume_panel) {
        if (!create_surface(&app, &app.panel, "jlinux-volume-button", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT, 54, PANEL_BODY_HEIGHT, -1, PANEL_MARGIN)) { goto cleanup; }
        zwlr_layer_surface_v1_set_margin(app.panel.layer, PANEL_MARGIN, 278, 0, 0);
        wl_surface_commit(app.panel.wl);
    }
    struct itimerspec timer = {.it_value = {.tv_sec = 30}, .it_interval = {.tv_sec = 30}};
    if (timerfd_settime(app.timer, 0, &timer, NULL)) { goto cleanup; }
    while (app.running) {
        if (wl_display_dispatch_pending(app.display) < 0) { goto cleanup; }
        if (!draw(&app.wallpaper) || !draw(&app.panel) || !draw(&app.power) || !draw(&app.audio) || !draw(&app.launcher)) { goto cleanup; }
        while (wl_display_prepare_read(app.display) != 0) {
            if (wl_display_dispatch_pending(app.display) < 0) { goto cleanup; }
        }
        int flushed = wl_display_flush(app.display);
        if (flushed < 0 && errno != EAGAIN) { wl_display_cancel_read(app.display); goto cleanup; }
        struct pollfd fds[] = {{wl_display_get_fd(app.display), POLLIN | (flushed < 0 ? POLLOUT : 0), 0},
            {app.timer, POLLIN, 0}, {app.signals, POLLIN, 0}, {app.control, POLLIN, 0}};
        int ready = poll(fds, 4, app.volume.pid ? 100 : -1);
        if (ready < 0) { wl_display_cancel_read(app.display); if (errno == EINTR) { continue; } goto cleanup; }
        if (fds[0].revents & POLLIN) { if (wl_display_read_events(app.display) < 0) { goto cleanup; } }
        else { wl_display_cancel_read(app.display); }
        if (app.volume.pid) {
            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            if ((now.tv_sec - app.volume.started.tv_sec) * 1000000000LL + now.tv_nsec - app.volume.started.tv_nsec >= 3000000000LL) { kill(app.volume.pid, SIGKILL); }
        }
        if (fds[3].revents & POLLIN) {
            char command[32];ssize_t count;
            while((count=recv(app.control,command,sizeof(command),0))>0){if(count==6 && !memcmp(command,"toggle",6)){toggle_launcher(&app,false);}}
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) { goto cleanup; }
        if (fds[1].revents & POLLIN) { uint64_t ticks; if (read(app.timer, &ticks, sizeof(ticks)) > 0) { app.panel.dirty = true; futurista_status(&app.futurista); if(app.launcher.wl){app.launcher.dirty=true;} if (app.audio.wl) { volume_start(&app.volume, 0); app.audio.dirty = true; } } }
        if (fds[2].revents & POLLIN) {
            struct signalfd_siginfo event;
            while (read(app.signals, &event, sizeof(event)) == sizeof(event)) {
                if (event.ssi_signo == SIGCHLD) {
                    int status; pid_t child;
                    while ((child = waitpid(-1, &status, WNOHANG)) > 0) {
                        if (child == app.volume.pid) {
                            volume_complete(&app.volume, status); app.launcher.dirty=app.launcher.wl!=NULL; app.audio.dirty = true;
                        } else if (child == app.power_pid) {
                            app.power_pid = 0;
                            if (!WIFEXITED(status) || WEXITSTATUS(status)) {
                                app.power_error = true;
                                fprintf(stderr, "Ação de sessão recusada (status %d).\n", status);
                                app.power_action = -1; app.power.dirty = true;
                            } else { close_power(&app); }
                        }
                    }
                }
                else { app.running = false; }
            }
        }
    }
    result = 0;
cleanup:
    if (result) { fprintf(stderr, "Shell falhou: Wayland=%d, errno=%d\n", app.display ? wl_display_get_error(app.display) : 0, errno); }
    shell_control_close(app.control);
    futurista_finish(&app.futurista);
    volume_finish(&app.volume);
    if (app.pointer) { wl_pointer_destroy(app.pointer); }
    if (app.seat) { wl_seat_destroy(app.seat); }
    if (app.cursor_surface) { wl_surface_destroy(app.cursor_surface); }
    if (app.cursor_theme) { wl_cursor_theme_destroy(app.cursor_theme); }
    struct surface *surfaces[] = {&app.panel, &app.wallpaper, &app.power, &app.audio, &app.launcher};
    for (size_t i = 0; i < 5; i++) {
        if (surfaces[i]->layer) { zwlr_layer_surface_v1_destroy(surfaces[i]->layer); }
        if (surfaces[i]->wl) { wl_surface_destroy(surfaces[i]->wl); }
    }
    struct buffer *buffer, *next;
    wl_list_for_each_safe(buffer, next, &app.buffers, link) { buffer_destroy(buffer); }
    /* Layer-shell v1 has no protocol destructor. */
    if (app.layers) { wl_proxy_destroy((struct wl_proxy *)app.layers); }
    if (app.shm) { wl_shm_destroy(app.shm); }
    if (app.compositor) { wl_compositor_destroy(app.compositor); }
    if (app.registry) { wl_registry_destroy(app.registry); }
    if (app.display) { wl_display_flush(app.display); wl_display_disconnect(app.display); }
    if (app.timer >= 0) { close(app.timer); }
    if (app.signals >= 0) { close(app.signals); }
    ui_finish();
    return result;
}
