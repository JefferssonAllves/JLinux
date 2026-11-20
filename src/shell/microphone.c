#define _GNU_SOURCE
#include "microphone.h"
#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static void child_signals(gpointer data) {
    (void)data; sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, NULL);
}
bool microphone_start(struct microphone *mic, bool playback, GError **error) {
    if (mic->pid || (playback && !mic->ready)) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                            mic->pid ? "Aguarde o teste terminar." : "Grave um teste antes de reproduzir.");
        return false;
    }
    if (!playback) {
        microphone_finish(mic);
        mic->directory = g_dir_make_tmp("jlinux-microphone-XXXXXX", error);
        if (!mic->directory) return false;
        mic->path = g_build_filename(mic->directory, "teste.wav", NULL);
        int fd = g_open(mic->path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
        if (fd < 0) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                        "Não foi possível criar a gravação: %s", g_strerror(errno));
            microphone_finish(mic); return false;
        }
        close(fd);
    }
    char *record[] = {"pw-record", "--rate", "48000", "--channels", "1", "--format", "s16", "--sample-count", "384000", mic->path, NULL};
    char *play[] = {"pw-play", mic->path, NULL};
    char **env = g_get_environ();
    if (!getenv("PIPEWIRE_RUNTIME_DIR")) {
        char *runtime = g_strdup_printf("/run/user/%lu", (unsigned long)getuid());
        env = g_environ_setenv(env, "PIPEWIRE_RUNTIME_DIR", runtime, true); g_free(runtime);
    }
    bool started = g_spawn_async(NULL, playback ? play : record, env,
        G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDOUT_TO_DEV_NULL,
        child_signals, NULL, &mic->pid, error);
    g_strfreev(env);
    if (!started) { mic->pid = 0; if (!playback) microphone_finish(mic); return false; }
    mic->playing = playback; mic->started = g_get_monotonic_time();
    return true;
}
bool microphone_complete(struct microphone *mic, int status) {
    bool ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!mic->playing) {
        struct stat info;
        /* pw-record returns 1 at its sample limit; require a complete eight-second WAV. */
        mic->ready = WIFEXITED(status) && WEXITSTATUS(status) <= 1 &&
            mic->path && !g_stat(mic->path, &info) && S_ISREG(info.st_mode) && info.st_size >= 48000 * 8 * 2 + 44;
        ok = mic->ready;
        if (!ok && mic->path) g_unlink(mic->path);
    }
    if (mic->pid) g_spawn_close_pid(mic->pid);
    mic->pid = 0; mic->playing = false; mic->started = 0;
    return ok;
}
void microphone_finish(struct microphone *mic) {
    if (mic->pid) {
        kill(mic->pid, SIGKILL);
        while (waitpid(mic->pid, NULL, 0) < 0 && errno == EINTR) {}
        g_spawn_close_pid(mic->pid);
    }
    if (mic->path) g_unlink(mic->path);
    if (mic->directory) g_rmdir(mic->directory);
    g_clear_pointer(&mic->path, g_free); g_clear_pointer(&mic->directory, g_free);
    mic->pid = 0; mic->playing = false; mic->ready = false; mic->started = 0;
}
