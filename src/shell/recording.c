#define _GNU_SOURCE
#include "recording.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static void child_signals(gpointer data) {
    (void)data; sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, NULL);
}
bool recording_toggle(struct recording *recording, GError **error) {
    if (recording->pid) {
        if (recording->stopping) return true;
        if (kill(recording->pid, SIGINT) && errno != ESRCH) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Não foi possível parar a gravação: %s", g_strerror(errno));
            return false;
        }
        recording->stopping = true;
        return true;
    }
    const char *videos = g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS);
    char *fallback = videos ? NULL : g_build_filename(g_get_home_dir(), "Videos", NULL);
    char *directory = g_build_filename(videos ? videos : fallback, "Gravações", NULL);
    g_free(fallback);
    if (g_mkdir_with_parents(directory, 0700)) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Não foi possível criar a pasta de gravações: %s", g_strerror(errno));
        g_free(directory); return false;
    }
    time_t now = time(NULL); struct tm local; localtime_r(&now, &local);
    char stamp[48]; strftime(stamp, sizeof(stamp), "Gravacao-%Y-%m-%d_%H-%M-%S-XXXXXX.mp4", &local);
    g_free(recording->path); recording->path = g_build_filename(directory, stamp, NULL); g_free(directory);
    int fd = mkstemps(recording->path, 4);
    if (fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Não foi possível criar o vídeo: %s", g_strerror(errno));
        g_clear_pointer(&recording->path, g_free); return false;
    }
    close(fd);
    char *argv[] = {"wf-recorder", "-y", "-f", recording->path, "-r", "30", "-c", "libx264", "-p", "preset=ultrafast", "-p", "crf=24", "-x", "yuv420p", NULL};
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDOUT_TO_DEV_NULL,
                      child_signals, NULL, &recording->pid, error)) {
        g_unlink(recording->path); g_clear_pointer(&recording->path, g_free); recording->pid = 0; return false;
    }
    recording->started = g_get_monotonic_time() / G_USEC_PER_SEC; recording->stopping = false;
    return true;
}
bool recording_complete(struct recording *recording, int status) {
    struct stat info;
    bool has_data = recording->path && !stat(recording->path, &info) && info.st_size > 0;
    if (recording->path && !has_data) g_unlink(recording->path);
    if (recording->pid) g_spawn_close_pid(recording->pid);
    recording->pid = 0; recording->stopping = false; recording->started = 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && has_data;
}
void recording_finish(struct recording *recording) {
    if (recording->pid) {
        kill(recording->pid, SIGINT);
        int status = 0; bool reaped = false;
        for (int i = 0; i < 80; i++) {
            pid_t result = waitpid(recording->pid, &status, WNOHANG);
            if (result == recording->pid || (result < 0 && errno == ECHILD)) { reaped = true; break; }
            struct timespec pause = {.tv_nsec = 25000000}; nanosleep(&pause, NULL);
        }
        if (!reaped) { kill(recording->pid, SIGKILL); while (waitpid(recording->pid, &status, 0) < 0 && errno == EINTR) {} }
        recording_complete(recording, status);
    }
    g_clear_pointer(&recording->path, g_free);
}
