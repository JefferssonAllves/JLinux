#define _GNU_SOURCE
#include "volume.h"
#include <fcntl.h>
#include <glib.h>
#include <math.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
/* shortcut: wpctl per request; use native subscriptions when adding live device selection. */
bool volume_start(struct volume *volume, int action) {
    if (volume->pid || action < 0 || action > 4) { return false; }
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK)) { volume->available = false; return false; }
    char *query[] = {"wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@", NULL};
    char *step[] = {"wpctl", "set-volume", "--limit", "1.0", "@DEFAULT_AUDIO_SINK@", action == 1 ? "5%-" : "5%+", NULL};
    char *mute[] = {"wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "toggle", NULL};
    char value[32]; snprintf(value, sizeof(value), "%.4f", volume->target);
    char *absolute[] = {"wpctl", "set-volume", "--limit", "1.0", "@DEFAULT_AUDIO_SINK@", value, NULL};
    char **args = action == 4 ? absolute : action == 0 ? query : action == 3 ? mute : step;
    char **env = g_get_environ();
    env = g_environ_setenv(env, "LC_ALL", "C", true);
    if (!getenv("PIPEWIRE_RUNTIME_DIR")) {
        char runtime[64]; snprintf(runtime, sizeof(runtime), "/run/user/%lu", (unsigned long)getuid());
        env = g_environ_setenv(env, "PIPEWIRE_RUNTIME_DIR", runtime, true);
    }
    posix_spawn_file_actions_t files;
    int error = posix_spawn_file_actions_init(&files);
    if (!error) {
        error = posix_spawn_file_actions_adddup2(&files, pipefd[1], STDOUT_FILENO);
        if (!error) { error = posix_spawn_file_actions_addopen(&files, STDERR_FILENO, "/dev/null", O_WRONLY, 0); }
        posix_spawnattr_t attributes;
        if (!error) {
            error = posix_spawnattr_init(&attributes);
            if (!error) {
                sigset_t empty; sigemptyset(&empty);
                error = posix_spawnattr_setsigmask(&attributes, &empty);
                if (!error) { error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK); }
                if (!error) { error = posix_spawnp(&volume->pid, args[0], &files, &attributes, args, env); }
                posix_spawnattr_destroy(&attributes);
            }
        }
        posix_spawn_file_actions_destroy(&files);
    }
    g_strfreev(env); close(pipefd[1]);
    if (error) { close(pipefd[0]); volume->pid = 0; volume->available = false; return false; }
    volume->fd = pipefd[0]; volume->query = action == 0;
    clock_gettime(CLOCK_MONOTONIC, &volume->started);
    return true;
}
bool volume_set(struct volume *volume, double level) {
    if (!isfinite(level)) { return false; }
    volume->target = fmin(1, fmax(0, level));
    volume->level = volume->target;
    volume->pending = true;
    if (volume->pid) { return true; }
    volume->pending = false;
    return volume_start(volume, 4);
}
void volume_complete(struct volume *volume, int status) {
    char output[128] = {0}, tail[32] = {0};
    ssize_t count = read(volume->fd, output, sizeof(output) - 1);
    close(volume->fd); volume->fd = -1; volume->pid = 0;
    if (!WIFEXITED(status) || WEXITSTATUS(status)) { volume->available = false; volume->pending = false; return; }
    if (volume->pending) { volume->pending = false; volume_start(volume, 4); return; }
    if (!volume->query) { volume_start(volume, 0); return; }
    double level;
    int fields = sscanf(output, "Volume: %lf %31[^\n]", &level, tail);
    volume->available = count > 0 && count < 127 && fields >= 1 && isfinite(level) && level >= 0 && level <= 10 &&
        (fields == 1 || !strcmp(tail, "[MUTED]"));
    if (volume->available) { volume->level = level; volume->muted = fields == 2; }
}
void volume_finish(struct volume *volume) {
    if (volume->pid) { kill(volume->pid, SIGKILL); waitpid(volume->pid, NULL, 0); close(volume->fd); volume->pid = 0; }
}
