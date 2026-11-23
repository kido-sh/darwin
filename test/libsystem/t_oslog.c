/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <os/log.h>
#include <os/signpost.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include "check.h"

extern char **environ;
extern char *_os_log_send_and_compose_impl(uint32_t flags, const char **format_out, char *buf,
    size_t size, void *dso, os_log_t log, os_log_type_t type, const char *format, uint8_t *pack,
    uint32_t pack_size);
extern void _os_crash_msg(const char *format, const char *message);

#define LOG_FILE "/var/log/system.log"

#define COMPOSE(flags, out, buf, size, fmt, ...) ({ \
        uint8_t pack_[__builtin_os_log_format_buffer_size(fmt, ##__VA_ARGS__)]; \
        __builtin_os_log_format(pack_, fmt, ##__VA_ARGS__); \
        _os_log_send_and_compose_impl(flags, out, buf, size, (void *)&__dso_handle, OS_LOG_DEFAULT, \
            OS_LOG_TYPE_ERROR, fmt, pack_, sizeof(pack_)); \
    })

static char *
read_file(const char *path)
{
    struct stat st;
    char *text;
    int fd = open(path, O_RDONLY);

    if (fd < 0 || fstat(fd, &st) != 0) {
        if (fd >= 0)
            close(fd);
        return strdup("");
    }
    text = calloc(1, (size_t)st.st_size + 1);
    if (read(fd, text, (size_t)st.st_size) < 0)
        text[0] = '\0';
    close(fd);
    return text;
}

static const char *
logged_line(const char *needle)
{
    static char line[4096];

    for (int i = 0; i < 40; i++) {
        char *text = read_file(LOG_FILE), *at = strstr(text, needle);

        if (at != NULL) {
            char *start = at, *end;

            while (start > text && start[-1] != '\n')
                start--;
            end = strchr(at, '\n');
            snprintf(line, sizeof(line), "%.*s", (int)(end != NULL ? end - start : (long)strlen(start)), start);
            free(text);
            return line;
        }
        free(text);
        usleep(50000);
    }
    return NULL;
}

static void
log_messages(const char *token)
{
    os_log_t log = os_log_create("sh.kido.test", "privacy");
    char *heap = strdup("heapstring");

    os_log(log, "%{public}s A lit=%s heap=%s pub=%{public}s int=%d ptr=%p obj=%@", token, "literal", heap, heap, 42,
        (void *)0x1234, NULL);
    os_log_error(log, "%{public}s B %{private}d %{public}d %s %{bool}d %{errno}d", token, 7, 8, __func__, 1, 2);
    os_log_debug(log, "%{public}s C debug is off", token);
    syslog(LOG_ERR, "%s D syslog heap=%s", token, heap);
    free(heap);
}

static char *
run_child(const char *mode, const char *token, int *status, int dt)
{
    char *argv[] = { (char *)getprogname(), (char *)mode, (char *)token, NULL };
    char *envp[] = { "OS_ACTIVITY_DT_MODE=YES", NULL };
    char out[] = "/tmp/kido-oslog.XXXXXX";
    posix_spawn_file_actions_t actions;
    char path[1024];
    uint32_t len = sizeof(path);
    pid_t pid;
    int fd = mkstemp(out);
    char *text;

    _NSGetExecutablePath(path, &len);
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fd, STDERR_FILENO);
    posix_spawn(&pid, path, &actions, NULL, argv, dt ? envp : (char *[]){ NULL });
    posix_spawn_file_actions_destroy(&actions);
    waitpid(pid, status, 0);
    close(fd);
    text = read_file(out);
    unlink(out);
    return text;
}


int
main(int argc, char **argv)
{
    char token[64], needle[96], buf[80], *text, *heap;
    const char *line, *format = NULL;
    os_log_t log;
    int status;

    if (argc == 3 && strcmp(argv[1], "log") == 0) {
        log_messages(argv[2]);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "crash") == 0) {
        _os_crash_msg("crash %s", argv[2]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "rotate") == 0) {
        char filler[1001];

        memset(filler, 'x', 1000);
        filler[1000] = '\0';
        for (int i = 0; i < 2600; i++) {
            syslog(LOG_NOTICE, "rotate %d %s", i, filler);
            if (i % 100 == 99)
                usleep(20000);
        }
        return 0;
    }

    snprintf(token, sizeof(token), "kido-oslog-%d-%ld", getpid(), (long)time(NULL));

    text = run_child("log", token, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(text[0] == '\0');
    free(text);
    snprintf(needle, sizeof(needle), "%s A ", token);
    line = logged_line(needle);
    CHECK(line != NULL && strstr(line, " Df ") != NULL && strstr(line, "[sh.kido.test:privacy] ") != NULL);
    CHECK(line != NULL && strstr(line, "A lit=literal heap=<private> pub=heapstring int=42 ptr=0x1234 obj=(null)") != NULL);
    snprintf(needle, sizeof(needle), "%s B ", token);
    line = logged_line(needle);
    CHECK(line != NULL && strstr(line, " E  ") != NULL);
    CHECK(line != NULL && strstr(line, "B <private> 8 log_messages true [2: No such file or directory]") != NULL);
    snprintf(needle, sizeof(needle), "%s C ", token);
    CHECK(logged_line(needle) == NULL);
    snprintf(needle, sizeof(needle), "%s D ", token);
    line = logged_line(needle);
    CHECK(line != NULL && strstr(line, "D syslog heap=heapstring") != NULL);

    snprintf(token, sizeof(token), "kido-oslog-dt-%d-%ld", getpid(), (long)time(NULL));
    text = run_child("log", token, &status, 1);
    CHECK(strstr(text, "] [privacy] ") != NULL);
    CHECK(strstr(text, "A lit=literal heap=heapstring pub=heapstring int=42") != NULL);
    CHECK(strstr(text, "B 7 8 log_messages true") != NULL);
    CHECK(strstr(text, "D syslog heap=heapstring") != NULL);
    free(text);

    log = os_log_create("sh.kido.test", "errno");
    errno = 1234;
    os_log(log, "errno %d", 5);
    CHECK(errno == 1234);
    CHECK(!os_log_type_enabled(OS_LOG_DISABLED, OS_LOG_TYPE_ERROR));

    heap = strdup("hello");
    CHECK(COMPOSE(2, &format, buf, sizeof(buf), "%s %{public}s %{private}s %d", heap, heap, heap, 5) == buf);
    CHECK(strcmp(buf, "<private> hello <private> 5") == 0);
    CHECK(format != NULL && strcmp(format, "%s %{public}s %{private}s %d") == 0);
    CHECK(COMPOSE(2, NULL, buf, sizeof(buf), "%.3s|%{public}.*s|%5d|%{public, uuid_t}.16P|%.2P|%{public}.2P", heap, 2,
        heap, 9, "\x01\x23\x45\x67\x89\xab\xcd\xef\x01\x23\x45\x67\x89\xab\xcd\xef", "\x0a\xff", "\x0a\xff") == buf);
    CHECK(strcmp(buf, "<private>|he|    9|01234567-89AB-CDEF-0123-456789ABCDEF|<private>|'0a ff'") == 0);
    free(heap);
    heap = calloc(1, 201);
    memset(heap, 'a', 200);
    text = COMPOSE(2, NULL, buf, sizeof(buf), "%{public}s", heap);
    CHECK(text != NULL && text != buf && strlen(text) == 200);
    free(text);
    free(heap);
    format = NULL;
    CHECK(COMPOSE(0, &format, buf, sizeof(buf), "plain") == NULL);
    CHECK(format != NULL && strcmp(format, "plain") == 0);
    errno = ENOENT;
    CHECK(COMPOSE(2, NULL, buf, sizeof(buf), "%m %{public}m %{private}m") == buf);
    CHECK(strcmp(buf, "No such file or directory No such file or directory <private>") == 0);
    CHECK(errno == ENOENT);

    CHECK(!os_signpost_enabled(log));
    CHECK(os_signpost_id_generate(log) == OS_SIGNPOST_ID_NULL);
    CHECK(os_signpost_id_make_with_pointer(log, &status) == OS_SIGNPOST_ID_NULL);
    os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "event", "%d", 1);
    _os_signpost_emit_with_name_impl((void *)&__dso_handle, log, OS_SIGNPOST_EVENT, OS_SIGNPOST_ID_EXCLUSIVE, "n", "f", NULL, 0);

    snprintf(token, sizeof(token), "kido-crash-%d-%ld", getpid(), (long)time(NULL));
    text = run_child("crash", token, &status, 0);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    snprintf(needle, sizeof(needle), "fatal: %s\n", token);
    CHECK(strcmp(text, needle) == 0);
    free(text);
    line = logged_line(token);
    CHECK(line != NULL && strstr(line, " F  ") != NULL);

    return checks_done("oslog");
}
