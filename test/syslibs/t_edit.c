/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <histedit.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "../libsystem/check.h"

static const char *
prompt(EditLine *el)
{
    (void)el;
    return "kido-el> ";
}

/* libedit leaves canonical mode after it prints the prompt: wait for that before typing */
static int
editing(int master)
{
    struct termios t;

    for (int i = 0; i < 500; i++) {
        if (tcgetattr(master, &t) == 0 && !(t.c_lflag & ICANON))
            return 1;
        usleep(10000);
    }
    return 0;
}

/*
 * line editing on a terminal: the child edits what the parent types, then reports through a pipe;
 * it runs before this process sets up readline without a terminal, which the child would inherit
 */
static void
edit_on_pty(void)
{
    char buf[16384];
    const char *line;
    size_t len = 0;
    ssize_t n;
    int master, pfd[2], status, step = 0, count = 0;
    struct winsize ws = { .ws_row = 24, .ws_col = 80 };
    pid_t pid;

    pipe(pfd);
    pid = forkpty(&master, NULL, NULL, &ws);
    if (pid == 0) {
        EditLine *el;
        char *r;
        FILE *out;

        close(pfd[0]);
        out = fdopen(pfd[1], "w");
        setenv("TERM", "xterm-256color", 1);
        el = el_init("kido", stdin, stdout, stderr);
        el_set(el, EL_PROMPT, prompt);
        el_set(el, EL_EDITOR, "emacs");
        line = el_gets(el, &count);
        fprintf(out, "el_gets=%s", line != NULL ? line : "(null)\n");
        el_end(el);
        r = readline("kido-rl> ");
        fprintf(out, "readline=%s\n", r != NULL ? r : "(null)");
        free(r);
        fclose(out);
        _exit(0);
    }
    close(pfd[1]);
    while (poll(&(struct pollfd){ .fd = master, .events = POLLIN }, 1, 20000) == 1 &&
           (n = read(master, buf + len, sizeof(buf) - 1 - len)) > 0) {
        len += n;
        buf[len] = '\0';
        if (step == 0 && strstr(buf, "kido-el> ") != NULL && editing(master)) {
            write(master, "abc\002Z\r", 6);
            step = 1;
        } else if (step == 1 && strstr(buf, "kido-rl> ") != NULL && editing(master)) {
            write(master, "foo\001x\r", 6);
            step = 2;
        }
    }
    if (step != 2) {
        printf("terminal output at step %d:", step);
        for (size_t i = 0; i < len; i++)
            printf(isprint((unsigned char)buf[i]) ? "%c" : "\\x%02x", (unsigned char)buf[i]);
        printf("\n");
        kill(pid, SIGKILL);
    }
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0 && step == 2);
    {
        FILE *in = fdopen(pfd[0], "r");
        char got[512] = "";

        fread(got, 1, sizeof(got) - 1, in);
        fclose(in);
        CHECK(strstr(got, "el_gets=abZc\n") != NULL);
        CHECK(strstr(got, "readline=xfoo\n") != NULL);
        if (strstr(got, "el_gets=abZc\n") == NULL || strstr(got, "readline=xfoo\n") == NULL)
            printf("child reported: %s", got);
    }
}

/* readline without a terminal reads plain lines, as python's input() does under a pipe */
static void
readline_from_pipe(void)
{
    int in[2], out[2], status;
    char got[256] = "";
    pid_t pid;

    pipe(in);
    pipe(out);
    pid = fork();
    if (pid == 0) {
        char *a, *b;
        FILE *f;

        dup2(in[0], 0);
        close(in[1]);
        close(out[0]);
        f = fdopen(out[1], "w");
        a = readline("");
        b = readline("");
        fprintf(f, "%s|%s|%s", a ? a : "(null)", b ? b : "(null)", readline("") ? "more" : "eof");
        fclose(f);
        _exit(0);
    }
    close(in[0]);
    close(out[1]);
    write(in[1], "first line\nsecond\n", 18);
    close(in[1]);
    read(out[0], got, sizeof(got) - 1);
    close(out[0]);
    waitpid(pid, &status, 0);
    CHECK(strcmp(got, "first line|second|eof") == 0);
}

int
main(void)
{
    char path[] = "/tmp/kido-history.XXXXXX", **matches, *exp = NULL;
    const char **argv;
    HIST_ENTRY *e;
    Tokenizer *tok;
    int argc, fd;

    alarm(120);
    edit_on_pty();
    readline_from_pipe();

    CHECK(strcmp(rl_library_version, "EditLine wrapper") == 0);
    CHECK(rl_readline_version == 0x0402);

    using_history();
    CHECK(add_history("one") == 0 && add_history("two") == 0 && add_history("three") == 0);
    CHECK(history_length == 3);
    e = history_get(history_base);
    CHECK(e != NULL && strcmp(e->line, "one") == 0);
    e = history_get(history_base + history_length - 1);
    CHECK(e != NULL && strcmp(e->line, "three") == 0);
    CHECK(history_expand("echo !!", &exp) == 1 && strcmp(exp, "echo three") == 0);
    free(exp);
    fd = mkstemp(path);
    close(fd);
    CHECK(write_history(path) == 0);
    clear_history();
    CHECK(history_length == 0);
    CHECK(read_history(path) == 0 && history_length == 3);
    stifle_history(2);
    CHECK(history_length == 2 && history_is_stifled());
    unlink(path);

    tok = tok_init(NULL);
    CHECK(tok_str(tok, "a 'b c' d", &argc, &argv) == 0 && argc == 3 && strcmp(argv[1], "b c") == 0);
    tok_end(tok);

    chdir("/etc");
    matches = rl_completion_matches("host", rl_filename_completion_function);
    CHECK(matches != NULL && strncmp(matches[0], "hosts", 5) == 0);
    return checks_done("edit");
}
