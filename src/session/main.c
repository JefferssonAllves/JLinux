#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t spawn(char *const argv[], int output, int ready_fd) {
    pid_t pid = fork();
    if (pid != 0) { return pid; }
    if (setsid() < 0) { _exit(126); }
    sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, NULL);
    if (output >= 0) { dup2(output, STDOUT_FILENO); dup2(output, STDERR_FILENO); }
    if (ready_fd >= 0) {
        char value[24]; snprintf(value, sizeof(value), "%d", ready_fd);
        setenv("JLINUX_READY_FD", value, 1); fcntl(ready_fd, F_SETFD, 0);
    } else { unsetenv("JLINUX_READY_FD"); }
    execvp(argv[0], argv); perror(argv[0]); _exit(127);
}
static void print_log(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) { return; }
    char line[512]; while (fgets(line, sizeof(line), file)) { fputs(line, stderr); } fclose(file);
}
int main(int argc, char **argv) {
    bool drm = false, headless = false, codex = false, terminal = true;
    double duration = 0;
    const char *layout = "br";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--drm")) { drm = true; }
        else if (!strcmp(argv[i], "--headless")) { headless = true; }
        else if (!strcmp(argv[i], "--codex")) { codex = true; }
        else if (!strcmp(argv[i], "--no-terminal")) { terminal = false; }
        else if (!strcmp(argv[i], "--keyboard-layout") && i + 1 < argc) { layout = argv[++i]; }
        else if (!strcmp(argv[i], "--duration") && i + 1 < argc) {
            char *end; duration = strtod(argv[++i], &end);
            if (*end || !isfinite(duration) || duration <= 0 || duration > 86400) {
                fprintf(stderr, "Duração inválida (0 < segundos <= 86400).\n"); return 1;
            }
        } else {
            fprintf(stderr, "Uso: jlinux-session [--drm|--headless] [--codex] [--no-terminal] [--duration segundos] [--keyboard-layout br|us]\n");
            return !strcmp(argv[i], "--help") ? 0 : 1;
        }
    }
    if (drm && headless) { fprintf(stderr, "Escolha --drm ou --headless.\n"); return 1; }
    const char *parent_display = getenv("WAYLAND_DISPLAY"), *parent_runtime = getenv("XDG_RUNTIME_DIR");
    char parent_socket[PATH_MAX];
    if (drm && (parent_display || getenv("DISPLAY"))) { fprintf(stderr, "Execute --drm a partir do TTY.\n"); return 1; }
    if (!drm && !headless) {
        if (!parent_display || !parent_runtime) { fprintf(stderr, "Use --drm no TTY ou execute em Wayland.\n"); return 1; }
        if (snprintf(parent_socket, sizeof(parent_socket), "%s/%s", parent_runtime, parent_display) >= (int)sizeof(parent_socket)) { return 1; }
        if (parent_display[0] == '/') { snprintf(parent_socket, sizeof(parent_socket), "%s", parent_display); }
        setenv("WAYLAND_DISPLAY", parent_socket, 1);
    }
    char executable[PATH_MAX], compositor[PATH_MAX], shell[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length < 0) { return 1; } executable[length] = 0;
    char *slash = strrchr(executable, '/'); if (!slash) { return 1; } *slash = 0;
    if (snprintf(compositor, sizeof(compositor), "%s/jlinux-compositor", executable) >= (int)sizeof(compositor) ||
        snprintf(shell, sizeof(shell), "%s/jlinux-shell", executable) >= (int)sizeof(shell)) { return 1; }
    if (access(compositor, X_OK) || access(shell, X_OK)) { fprintf(stderr, "Binários da sessão não encontrados.\n"); return 1; }
    const char *home = getenv("HOME"), *path = getenv("PATH");
    char search_path[16384];
    if (home && snprintf(search_path, sizeof(search_path), "%s/.local/bin:%s", home, path ? path : "/usr/bin") < (int)sizeof(search_path)) {
        setenv("PATH", search_path, 1);
    }
    char runtime[] = "/tmp/jlinux-session-XXXXXX";
    if (!mkdtemp(runtime)) { return 1; }
    setenv("XDG_RUNTIME_DIR", runtime, 1);
    setenv("XKB_DEFAULT_LAYOUT", layout, 1);
    setenv("WLR_BACKENDS", headless ? "headless" : drm ? "drm,libinput" : "wayland", 1);
    if (headless) { setenv("WLR_RENDERER", "pixman", 1); setenv("WLR_HEADLESS_OUTPUTS", "1", 1); }
    else { unsetenv("WLR_RENDERER"); }
    sigset_t signals; sigemptyset(&signals); sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM); sigaddset(&signals, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &signals, NULL)) { rmdir(runtime); return 1; }
    int signal_fd = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    int ready[2] = {-1, -1}, logs[2] = {-1, -1};
    char log_paths[2][PATH_MAX], socket_name[128] = {0};
    snprintf(log_paths[0], sizeof(log_paths[0]), "%s/compositor.log", runtime);
    snprintf(log_paths[1], sizeof(log_paths[1]), "%s/shell.log", runtime);
    pid_t children[3] = {0};
    int result = 1;
    if (signal_fd < 0 || timer_fd < 0 || pipe2(ready, O_CLOEXEC)) { goto cleanup; }
    logs[0] = open(log_paths[0], O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    logs[1] = open(log_paths[1], O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (logs[0] < 0 || logs[1] < 0) { goto cleanup; }
    char *server_args[] = {compositor, NULL}; children[0] = spawn(server_args, logs[0], ready[1]);
    close(ready[1]); ready[1] = -1;
    if (children[0] < 0) { goto cleanup; }
    struct pollfd startup[] = {{ready[0], POLLIN, 0}, {signal_fd, POLLIN, 0}};
    if (poll(startup, 2, 8000) <= 0 || !(startup[0].revents & POLLIN)) { goto cleanup; }
    ssize_t count = read(ready[0], socket_name, sizeof(socket_name) - 1);
    if (count <= 0 || strchr(socket_name, '/')) { goto cleanup; }
    close(ready[0]); ready[0] = -1; setenv("WAYLAND_DISPLAY", socket_name, 1);
    char *shell_args[] = {shell, NULL}; children[1] = spawn(shell_args, logs[1], -1);
    if (children[1] < 0) { goto cleanup; }
    if (terminal) {
        char *plain[] = {"foot", "--term=xterm-256color", "--title=JLinux Terminal", NULL};
        char *assistant[] = {"foot", "--term=xterm-256color", "--title=JLinux Codex", "codex", NULL};
        children[2] = spawn(codex ? assistant : plain, logs[1], -1);
        if (children[2] < 0) { goto cleanup; }
    }
    if (duration > 0) {
        struct itimerspec timer = {.it_value = {.tv_sec = (time_t)duration, .tv_nsec = (long)((duration - (time_t)duration) * 1e9)}};
        if (!timer.it_value.tv_sec && !timer.it_value.tv_nsec) { timer.it_value.tv_nsec = 1; }
        if (timerfd_settime(timer_fd, 0, &timer, NULL)) { goto cleanup; }
    }
    printf("JLinux nativo iniciado. Super+Enter: terminal. Super+Esc: sair.\nLogs: %s\n", runtime); fflush(stdout);
    for (;;) {
        struct pollfd events[] = {{signal_fd, POLLIN, 0}, {timer_fd, POLLIN, 0}};
        if (poll(events, 2, -1) < 0) { if (errno == EINTR) { continue; } goto cleanup; }
        if (events[1].revents & POLLIN) { result = 0; break; }
        struct signalfd_siginfo event;
        while (read(signal_fd, &event, sizeof(event)) == sizeof(event)) {
            if (event.ssi_signo != SIGCHLD) { result = 0; goto cleanup; }
            int status; pid_t pid;
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
                if (pid == children[0]) {
                    children[0] = 0; result = WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1; goto cleanup;
                }
                if (pid == children[1]) { children[1] = 0; goto cleanup; }
                if (pid == children[2]) {
                    children[2] = 0;
                    if (!WIFEXITED(status) || WEXITSTATUS(status)) { fprintf(stderr, "Terminal encerrou com erro.\n"); }
                }
            }
        }
    }
cleanup:
    for (int i = 2; i >= 0; i--) { if (children[i] > 0) { kill(-children[i], SIGTERM); } }
    for (int attempt = 0; attempt < 30; attempt++) {
        bool live = false;
        for (int i = 0; i < 3; i++) {
            if (children[i] > 0) {
                if (waitpid(children[i], NULL, WNOHANG) == 0) { live = true; }
                else { children[i] = 0; }
            }
        }
        if (!live) { break; }
        struct pollfd event = {signal_fd, POLLIN, 0}; poll(&event, 1, 100);
        struct signalfd_siginfo ignored; while (read(signal_fd, &ignored, sizeof(ignored)) > 0) { }
    }
    for (int i = 0; i < 3; i++) { if (children[i] > 0) { kill(-children[i], SIGKILL); waitpid(children[i], NULL, 0); } }
    if (result) { fprintf(stderr, "Falha na sessão:\n"); print_log(log_paths[0]); print_log(log_paths[1]); }
    for (int i = 0; i < 2; i++) {
        if (logs[i] >= 0) { close(logs[i]); }
        if (ready[i] >= 0) { close(ready[i]); }
        unlink(log_paths[i]);
    }
    if (signal_fd >= 0) { close(signal_fd); } if (timer_fd >= 0) { close(timer_fd); }
    if (socket_name[0]) {
        char socket_path[PATH_MAX]; snprintf(socket_path, sizeof(socket_path), "%s/%s", runtime, socket_name); unlink(socket_path);
        snprintf(socket_path, sizeof(socket_path), "%s/%s.lock", runtime, socket_name); unlink(socket_path);
    }
    if (rmdir(runtime)) { fprintf(stderr, "Arquivos de diagnóstico preservados em %s\n", runtime); }
    return result;
}
