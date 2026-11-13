#define _GNU_SOURCE
#include "../toolkit/painter.h"
#include "volume.h"
#include "bluetooth.h"
#include "apps.h"
#include "preferences.h"
#include <glib.h>
#include <gio/gdesktopappinfo.h>
#include "futurista.h"
#include "control.h"
#include <sys/socket.h>
#include "../effects/glass.h"
#include "../animation/transition.h"
#include "../renderer/dock-transform.h"
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
enum { PANEL_BODY_HEIGHT = 56, PANEL_FADE_HEIGHT = 0,
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
    bool volume_dragging, showing_controls, dock_closing, reduced_motion;
    struct transition dock_open, dock_hover[10];
    struct wl_callback *dock_frame;
    int control;
    struct volume volume;
    struct bluetooth bluetooth;
    struct media media;
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
static double animation_now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1000000000.0;
}
static void destroy_launcher(struct app *app);
static bool dock_animating(struct app *app, double now) {
    if (!app->launcher.wl || app->showing_controls) { return false; }
    if (transition_active(&app->dock_open, now)) { return true; }
    for (int i = 0; i < 10; i++) { if (transition_active(&app->dock_hover[i], now)) { return true; } }
    return false;
}
static void dock_frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    (void)time; struct app *app = data;
    wl_callback_destroy(callback); app->dock_frame = NULL;
    double now = animation_now();
    if (app->dock_closing && !transition_active(&app->dock_open, now)) { destroy_launcher(app); return; }
    app->launcher.dirty = true;
}
static const struct wl_callback_listener dock_frame_listener = {.done = dock_frame_done};
static void dock_select(struct app *app, int hit) {
    double now = animation_now();
    for (int i = 0; i < 10; i++) {
        transition_target(&app->dock_hover[i], i == hit ? 1 : 0, now, app->reduced_motion || (!app->futurista.prefs.zoom && !app->futurista.prefs.neon) ? 0 : .14);
    }
    app->launcher.dirty = true;
}
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
        struct app *app = surface->app;
        double now = animation_now(), progress = transition_value(&app->dock_open, now);
        for (int i = 0; i < 10; i++) { app->futurista.hover[i] = transition_value(&app->dock_hover[i], now); }
        if (!app->showing_controls) {
            cairo_save(cr);
            struct ui_rect box = futurista_dock_bounds(width, height);
            cairo_rectangle(cr, box.x, box.y, box.width, box.height); cairo_clip(cr);
            cairo_matrix_t matrix; jlinux_dock_transform(&matrix, width, height, progress);
            cairo_transform(cr, &matrix);
            cairo_push_group(cr);
        }
        futurista_overlay(&app->futurista, cr, width, height, app->showing_controls);
        if (!app->showing_controls) {
            cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, progress); cairo_restore(cr);
        }
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
    /* Closing needs a final callback even if rasterization outlasted the transition. */
    bool animate = surface == &surface->app->launcher && (surface->app->dock_closing || dock_animating(surface->app, animation_now()));
    cairo_t *cr = cairo_create(image); paint(surface, cr);
    bool valid = cairo_status(cr) == CAIRO_STATUS_SUCCESS && cairo_surface_status(image) == CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr); cairo_surface_destroy(image);
    if (!valid) { buffer_destroy(buffer); return false; }
    wl_surface_attach(surface->wl, buffer->wl, 0, 0);
    wl_surface_damage_buffer(surface->wl, 0, 0, surface->width, surface->height);
    if (surface == &surface->app->launcher && !surface->app->dock_frame && animate) {
        surface->app->dock_frame = wl_surface_frame(surface->wl);
        if (!surface->app->dock_frame) { return false; }
        wl_callback_add_listener(surface->app->dock_frame, &dock_frame_listener, surface->app);
    }
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
static void destroy_launcher(struct app *app) {
    if (app->dock_frame) { wl_callback_destroy(app->dock_frame); app->dock_frame = NULL; }
    app->dock_closing = false;
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
static void close_launcher(struct app *app) {
    app->volume_dragging = false;
    if (!app->launcher.wl || app->dock_closing) { return; }
    if (app->showing_controls || app->reduced_motion) { destroy_launcher(app); return; }
    app->dock_closing = true;
    transition_target(&app->dock_open, 0, animation_now(), .16);
    struct wl_region *empty = wl_compositor_create_region(app->compositor);
    if (!empty) { destroy_launcher(app); return; }
    wl_surface_set_input_region(app->launcher.wl, empty); wl_region_destroy(empty);
    app->launcher.dirty = true;
}
static void toggle_launcher(struct app *app, bool controls) {
    if(app->launcher.wl){
        bool same=app->showing_controls==controls;
        if(same && !controls){close_launcher(app);return;}
        destroy_launcher(app);
        if(same){return;}
    }
    app->showing_controls=controls;
    app->futurista.settings_open=false;
    app->dock_open = (struct transition){0};
    transition_target(&app->dock_open, 1, animation_now(), app->reduced_motion ? 0 : .20);
    for(int i=0;i<10;i++){app->dock_hover[i]=(struct transition){0};}
    dock_select(app,app->futurista.selected);
    uint32_t all = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    if (!create_surface(app, &app->launcher, controls?"jlinux-control-center":"jlinux-radial-dock", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, all, 0,
                        0, -1, 0)) {
        app->running = false;
        return;
    }
    futurista_status(&app->futurista);
    if(controls){volume_start(&app->volume, 0);bluetooth_start(&app->bluetooth,-1);}
    app->futurista.message[0] = 0;
    fprintf(stderr,"%s\n",controls?"Painel de controle aberto.":"Dock circular aberto.");
}
static void launch_item(struct app *app, int index) {
    if(index<0 || index>=10){return;}
    if(index==5){
        if(app->launcher.wl){destroy_launcher(app);}
        toggle_launcher(app,true);
        app->futurista.settings_open=true;
        app->futurista.settings_category=0;
        app->launcher.dirty=true;
        fprintf(stderr,"Configurações abertas.\n");
        return;
    }
    GError *error=NULL;
    if(!dock_app_launch(index,&error)){
        snprintf(app->futurista.message,sizeof(app->futurista.message),"%s",error?error->message:"Falha ao abrir aplicativo");
        g_clear_error(&error);app->launcher.dirty=true;
    }else{fprintf(stderr,"Aplicativo iniciado: %s\n",dock_app_name(index));}
}
static void apply_preferences(struct app *app) {
    struct shell_preferences p=app->futurista.prefs;
    app->reduced_motion=!p.animations || (getenv("JLINUX_REDUCED_MOTION") && !strcmp(getenv("JLINUX_REDUCED_MOTION"),"1"));
    app->futurista.glass.tint=p.tint;
    app->futurista.glass.reflection=p.reflection;
    app->futurista.glass.shadow=p.shadow;
    app->panel.dirty=true;app->launcher.dirty=true;
}
static bool bluetooth_manager_open(GError **error) {
    /* The session creates its private Wayland socket after the D-Bus daemon starts. */
    GDBusConnection *bus=g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,error);
    if(!bus){return false;}
    GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("a{ss}"));
    const char *names[]={"WAYLAND_DISPLAY","XDG_RUNTIME_DIR"};
    for(unsigned i=0;i<2;i++){const char *value=getenv(names[i]);if(value){g_variant_builder_add(&values,"{ss}",names[i],value);}}
    GVariant *reply=g_dbus_connection_call_sync(bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","UpdateActivationEnvironment",g_variant_new("(a{ss})",&values),G_VARIANT_TYPE("()"),G_DBUS_CALL_FLAGS_NONE,1000,NULL,error);
    g_object_unref(bus);if(!reply){return false;}g_variant_unref(reply);
    GDesktopAppInfo *info=g_desktop_app_info_new("blueman-manager.desktop");
    if(!info){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND,"Instale o gerenciador Blueman.");return false;}
    bool opened=g_app_info_launch(G_APP_INFO(info),NULL,NULL,error);g_object_unref(info);return opened;
}
static void launcher_action(struct app *app, int hit) {
    if(app->futurista.settings_open && hit>=100 && hit<=102){app->futurista.settings_category=hit-100;app->launcher.dirty=true;return;}
    if(app->futurista.settings_open && ((hit>=110 && hit<=112)||(hit>=120 && hit<=122))){
        struct shell_preferences old=app->futurista.prefs;
        struct shell_preferences *p=&app->futurista.prefs;
        bool *values[]={&p->animations,&p->zoom,&p->neon,&p->tint,&p->reflection,&p->shadow};
        int index=hit>=120?hit-120+3:hit-110;
        *values[index]=!*values[index];
        GError *error=NULL;
        if(!shell_preferences_save(p,&error)){
            *p=old;snprintf(app->futurista.message,sizeof(app->futurista.message),"Não foi possível salvar: %s",error->message);g_clear_error(&error);
        }else{app->futurista.message[0]=0;apply_preferences(app);fprintf(stderr,"Preferências do shell salvas.\n");}
        app->launcher.dirty=true;return;
    }
    if(app->futurista.settings_open && hit==130){app->futurista.settings_open=false;volume_start(&app->volume,0);app->launcher.dirty=true;return;}
    if(app->futurista.settings_open && (hit==131 || hit==132)){launch_item(app,hit==131?4:8);return;}
    if(hit==40){return;}
    if(hit==21){
        if(!app->bluetooth.pid){bluetooth_start(&app->bluetooth,app->bluetooth.available ? !app->bluetooth.powered : -1);}
        app->launcher.dirty=true;return;
    }
    if(hit==36){
        GError *error=NULL;
        bool opened=bluetooth_manager_open(&error);
        if(opened){close_launcher(app);}
        else{snprintf(app->futurista.message,sizeof(app->futurista.message),"%s",error?error->message:"Instale o gerenciador Blueman.");app->launcher.dirty=true;}
        g_clear_error(&error);return;
    }

    if (hit >= 0 && hit < 10) {
        app->futurista.selected = hit;
        dock_select(app,hit);
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
static int launcher_hit(struct app *app) {
    if (app->dock_closing) { return -1; }
    double x = app->pointer_x, y = app->pointer_y;
    if (!app->showing_controls) {
        cairo_matrix_t matrix;
        jlinux_dock_transform(&matrix, app->launcher.width, app->launcher.height, transition_value(&app->dock_open, animation_now()));
        if (cairo_matrix_invert(&matrix) != CAIRO_STATUS_SUCCESS) { return -1; }
        cairo_matrix_transform_point(&matrix, &x, &y);
    }
    if(app->futurista.settings_open){return futurista_settings_hit(x,y,app->launcher.width,app->launcher.height,app->futurista.settings_category);}
    return futurista_hit(x,y,app->launcher.width,app->launcher.height,app->showing_controls);
}
static void update_launcher_selection(struct app *app) {
    if(app->pointer_surface != &app->launcher){return;}
    int hit=launcher_hit(app);
    if(hit>=0 && hit<10 && hit!=app->futurista.selected){
        app->futurista.selected=hit;
        if(!app->showing_controls){dock_select(app,hit);}
        app->launcher.dirty=true;
    }
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
    update_launcher_selection(app);
}
static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *wl) {
    (void)pointer; (void)serial; (void)wl;
    struct app *app = data;
    app->pointer_surface = NULL;
}
static void drag_volume(struct app *app) {
    volume_set(&app->volume, futurista_volume_at(app->pointer_x, app->launcher.width, app->launcher.height));
    app->launcher.dirty = true;
}
static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)pointer; (void)time;
    struct app *app = data; app->pointer_x = wl_fixed_to_double(x); app->pointer_y = wl_fixed_to_double(y);
    if (app->volume_dragging) { drag_volume(app); }
    update_launcher_selection(app);
}
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void)pointer; (void)serial; (void)time;
    struct app *app = data;
    if (button != BTN_LEFT) { return; }
    if (app->volume_dragging) {
        if (state == WL_POINTER_BUTTON_STATE_RELEASED) { drag_volume(app); app->volume_dragging = false; }
        return;
    }
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (app->pointer_surface == &app->launcher && app->showing_controls && launcher_hit(app) == 30) {
            app->volume_dragging = true; drag_volume(app);
        }
        return;
    }
    if (app->pointer_surface == &app->launcher) {
        if(!app->dock_closing){launcher_action(app,launcher_hit(app));}
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
            int media_hit=futurista_media_hit(app->pointer_x,app->pointer_y,app->panel.width);
            if(media_hit>=0){media_action(&app->media,media_hit);}
            else if(app->pointer_x>=app->panel.width-60){toggle_launcher(app,true);}

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
static int preview_futurista(const char *path, bool controls, bool settings, bool dock_crop) {
    struct futurista scene;
    struct jlinux_glass effects;
    if (!jlinux_glass_settings(&effects)) { return 1; }
    if (!futurista_init(&scene,effects)) { futurista_finish(&scene); return 1; }
    scene.wifi = 1;
    scene.battery = 75;
    scene.charging = true; /* Illustrative status for the component preview. */
    scene.settings_open = settings;
    scene.volume = .40;
    scene.brightness = .53;
    cairo_surface_t *image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dock_crop ? 1254 : 1672, dock_crop ? 1254 : 941);
    cairo_t *cr = cairo_create(image);
    if(dock_crop){
        scene.hover[0]=0;scene.selected=9;scene.hover[9]=1;
        cairo_set_source_rgb(cr,17/255.,16/255.,28/255.);cairo_paint(cr);
        cairo_scale(cr,1254/700.,1254/700.);cairo_translate(cr,-486,-119);
    }else{
        futurista_wallpaper(&scene, cr, 1672, 941);
        futurista_panel(&scene, cr, 1672);
    }
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
    if (argc == 3 && !strcmp(argv[1], "--preview-dock")) { return preview_futurista(argv[2],false,false,true); }
    if (argc == 3 && !strcmp(argv[1], "--preview-settings")) { return preview_futurista(argv[2],true,true,false); }
    if (argc == 3 && !strcmp(argv[1], "--preview-control-center")) { return preview_futurista(argv[2],true,false,false); }
    if (argc == 3 && !strcmp(argv[1], "--preview-futurista")) { return preview_futurista(argv[2],false,false,false); }
    if (argc == 3 && !strcmp(argv[1], "--preview")) { return preview(argv[2]); }
    if (argc != 1 && !power_panel && !volume_panel) { fprintf(stderr, "Uso: jlinux-shell [--preview arquivo.png | --preview-power arquivo.png | --power-panel | --volume-panel]\n"); return 1; }
    struct app app = {.running = true, .power_panel = power_panel, .volume_panel = volume_panel, .power_action = -1, .timer = -1, .signals = -1, .control = -1}; wl_list_init(&app.buffers);
    /* GIO and SVG loaders can start threads; all must inherit the signal mask. */
    sigset_t signals; sigemptyset(&signals); sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM); sigaddset(&signals, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &signals, NULL)) { return 1; }
    if (getenv("JLINUX_THEME")) { fprintf(stderr, "JLINUX_THEME/CSS não é suportado pelo shell nativo.\n"); return 1; }
    if (!jlinux_glass_settings(&app.glass)) { return 1; }
    if (!futurista_init(&app.futurista,app.glass)) { futurista_finish(&app.futurista); return 1; }
    const char *reduced = getenv("JLINUX_REDUCED_MOTION");
    if (reduced && strcmp(reduced,"0") && strcmp(reduced,"1")) { fprintf(stderr,"JLINUX_REDUCED_MOTION deve ser 0 ou 1.\n"); futurista_finish(&app.futurista); return 1; }
    app.reduced_motion = reduced && !strcmp(reduced,"1");
    app.futurista.prefs.tint=app.glass.tint;app.futurista.prefs.reflection=app.glass.reflection;app.futurista.prefs.shadow=app.glass.shadow;
    GError *preferences_error=NULL;
    if(!shell_preferences_load(&app.futurista.prefs,&preferences_error)){fprintf(stderr,"Preferências ignoradas: %s\n",preferences_error->message);g_clear_error(&preferences_error);}
    apply_preferences(&app);
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
    app.futurista.bluetooth=-1;
    media_init(&app.media);app.futurista.media=&app.media;
    while (app.running) {
        while(g_main_context_iteration(NULL,false)){}
        if(app.media.dirty){app.panel.dirty=true;app.media.dirty=false;}
        app.futurista.bluetooth=app.bluetooth.available ? app.bluetooth.powered : -1;
        app.futurista.bluetooth_busy=app.bluetooth.pid!=0;
        if (wl_display_dispatch_pending(app.display) < 0) { goto cleanup; }
        if (!draw(&app.wallpaper) || !draw(&app.panel) || !draw(&app.power) || !draw(&app.audio) || !draw(&app.launcher)) { goto cleanup; }
        while (wl_display_prepare_read(app.display) != 0) {
            if (wl_display_dispatch_pending(app.display) < 0) { goto cleanup; }
        }
        int flushed = wl_display_flush(app.display);
        if (flushed < 0 && errno != EAGAIN) { wl_display_cancel_read(app.display); goto cleanup; }
        struct pollfd base_fds[] = {{wl_display_get_fd(app.display), POLLIN | (flushed < 0 ? POLLOUT : 0), 0},
            {app.timer, POLLIN, 0}, {app.signals, POLLIN, 0}, {app.control, POLLIN, 0}};
        GMainContext *context=g_main_context_default();
        if(!g_main_context_acquire(context)){wl_display_cancel_read(app.display);goto cleanup;}
        int priority, timeout;
        bool immediate=g_main_context_prepare(context,&priority);
        int count=g_main_context_query(context,priority,&timeout,NULL,0),capacity;
        g_autofree GPollFD *gio_fds=NULL;
        do {
            capacity=count;g_free(gio_fds);gio_fds=g_new0(GPollFD,capacity);
            count=g_main_context_query(context,priority,&timeout,gio_fds,capacity);
        } while(count>capacity);
        g_autofree struct pollfd *fds=g_new0(struct pollfd,4+count);
        memcpy(fds,base_fds,sizeof(base_fds));
        for(int i=0;i<count;i++){fds[i+4]=(struct pollfd){gio_fds[i].fd,gio_fds[i].events,0};}
        if(immediate){timeout=0;}
        if((app.volume.pid||app.bluetooth.pid)&&(timeout<0||timeout>100)){timeout=100;}
        int ready = poll(fds, 4+count, timeout);
        for(int i=0;i<count;i++){gio_fds[i].revents=fds[i+4].revents;}
        if(ready>=0&&g_main_context_check(context,priority,gio_fds,count)){g_main_context_dispatch(context);}
        g_main_context_release(context);
        if (ready < 0) { wl_display_cancel_read(app.display); if (errno == EINTR) { continue; } goto cleanup; }
        if (fds[0].revents & POLLIN) { if (wl_display_read_events(app.display) < 0) { goto cleanup; } }
        else { wl_display_cancel_read(app.display); }
        if (app.volume.pid) {
            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            if ((now.tv_sec - app.volume.started.tv_sec) * 1000000000LL + now.tv_nsec - app.volume.started.tv_nsec >= 3000000000LL) { kill(app.volume.pid, SIGKILL); }
        }
        if(app.bluetooth.pid){
            struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
            if((now.tv_sec-app.bluetooth.started.tv_sec)*1000000000LL+now.tv_nsec-app.bluetooth.started.tv_nsec>=5000000000LL){kill(app.bluetooth.pid,SIGKILL);}
        }
        if (fds[3].revents & POLLIN) {
            char command[32];ssize_t count;
            while((count=recv(app.control,command,sizeof(command),0))>0){
                if(count==4 && !memcmp(command,"show",4)){
                    if(app.launcher.wl && !app.showing_controls && app.dock_closing){
                        app.dock_closing=false;
                        transition_target(&app.dock_open,1,animation_now(),app.reduced_motion?0:.20);
                        struct wl_region *input=wl_compositor_create_region(app.compositor);
                        if(!input){goto cleanup;}
                        wl_region_add(input,0,PANEL_BODY_HEIGHT,8192,8192);
                        wl_surface_set_input_region(app.launcher.wl,input);wl_region_destroy(input);app.launcher.dirty=true;
                    } else if(!app.launcher.wl || app.showing_controls){toggle_launcher(&app,false);}
                }
                else if(count==4 && !memcmp(command,"hide",4) && app.launcher.wl && !app.showing_controls){close_launcher(&app);}
            }
        }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) { goto cleanup; }
        if (fds[1].revents & POLLIN) { uint64_t ticks; if (read(app.timer, &ticks, sizeof(ticks)) > 0) { app.panel.dirty = true; futurista_status(&app.futurista); if(app.launcher.wl && app.showing_controls){bluetooth_start(&app.bluetooth,-1);} if(app.launcher.wl){app.launcher.dirty=true;} if (app.audio.wl) { volume_start(&app.volume, 0); app.audio.dirty = true; } } }
        if (fds[2].revents & POLLIN) {
            struct signalfd_siginfo event;
            while (read(app.signals, &event, sizeof(event)) == sizeof(event)) {
                if (event.ssi_signo == SIGCHLD) {
                    int status; pid_t child;
                    while ((child = waitpid(-1, &status, WNOHANG)) > 0) {
                        if (child == app.volume.pid) {
                            volume_complete(&app.volume, status); app.launcher.dirty=app.launcher.wl!=NULL; app.audio.dirty = true;
                        } else if (child == app.bluetooth.pid) {
                            bluetooth_complete(&app.bluetooth,status);
                            if(app.bluetooth.error[0]){snprintf(app.futurista.message,sizeof(app.futurista.message),"%s",app.bluetooth.error);}
                            else{app.futurista.message[0]=0;}
                            app.launcher.dirty=app.launcher.wl!=NULL;
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
    if(app.dock_frame){wl_callback_destroy(app.dock_frame);app.dock_frame=NULL;}
    if (result) { fprintf(stderr, "Shell falhou: Wayland=%d, errno=%d\n", app.display ? wl_display_get_error(app.display) : 0, errno); }
    if(app.media.cancel){media_finish(&app.media);}
    shell_control_close(app.control);
    futurista_finish(&app.futurista);
    bluetooth_finish(&app.bluetooth);
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
