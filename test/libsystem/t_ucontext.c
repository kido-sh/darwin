/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#define _XOPEN_SOURCE 600
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include "check.h"

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#define STACK_SIZE (128 * 1024)

static ucontext_t main_ctx, func_ctx, gen_ctx;
static int args_seen[10];
static int nargs_seen;
static volatile int generated;
static double fp_seen;

static void
ten_args(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j)
{
    int v[10] = { a, b, c, d, e, f, g, h, i, j };

    memcpy(args_seen, v, sizeof(v));
    nargs_seen = 10;
}

static void
two_args(int a, int b)
{
    volatile double x = a * 1.5, y = b * 0.25;

    fp_seen = x + y;
    args_seen[0] = a;
    args_seen[1] = b;
    nargs_seen = 2;
}

static void
generator(void)
{
    for (int i = 1; i <= 5; i++) {
        generated = i;
        swapcontext(&gen_ctx, &main_ctx);
    }
    generated = -1;
}

static void
child_exits(int fd)
{
    (void)write(fd, "ran", 3);
}

typedef struct {
    ucontext_t fibre;
    jmp_buf env;
    int env_init;
} fibre_t;

static fibre_t fibre_main, fibre_job;
static int job_steps;

static void
fibre_swap(fibre_t *o, fibre_t *n, int r)
{
    o->env_init = 1;
    if (!r || !_setjmp(o->env)) {
        if (n->env_init)
            _longjmp(n->env, 1);
        else
            setcontext(&n->fibre);
    }
}

static void
job(void)
{
    for (;;) {
        job_steps++;
        fibre_swap(&fibre_job, &fibre_main, 1);
    }
}

static char *
new_stack(ucontext_t *uc, ucontext_t *link)
{
    char *stack = malloc(STACK_SIZE);

    CHECK(getcontext(uc) == 0);
    uc->uc_stack.ss_sp = stack;
    uc->uc_stack.ss_size = STACK_SIZE;
    uc->uc_link = link;
    return stack;
}

int
main(void)
{
    volatile int resumed = 0;
    volatile double kept = 3.25;
    ucontext_t uc;
    sigset_t blocked, now;
    int pipefd[2], status;
    pid_t pid;
    char buf[8] = { 0 };

    CHECK(getcontext(&uc) == 0);
    if (resumed++ < 3)
        setcontext(&uc);
    CHECK(resumed == 4);
    CHECK(uc.uc_mcontext == &uc.__mcontext_data);
    CHECK(uc.uc_mcsize == sizeof(uc.__mcontext_data));

    new_stack(&func_ctx, &main_ctx);
    makecontext(&func_ctx, (void (*)(void))ten_args, 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    CHECK(swapcontext(&main_ctx, &func_ctx) == 0);
    CHECK(nargs_seen == 10);
    for (int i = 0; i < 10; i++)
        CHECK(args_seen[i] == i + 1);
    CHECK(kept == 3.25);

    new_stack(&func_ctx, &main_ctx);
    makecontext(&func_ctx, (void (*)(void))two_args, 2, -7, 40);
    CHECK(swapcontext(&main_ctx, &func_ctx) == 0);
    CHECK(nargs_seen == 2 && args_seen[0] == -7 && args_seen[1] == 40);
    CHECK(fp_seen == -7 * 1.5 + 40 * 0.25);

    new_stack(&gen_ctx, &main_ctx);
    makecontext(&gen_ctx, generator, 0);
    for (int i = 1; i <= 5; i++) {
        CHECK(swapcontext(&main_ctx, &gen_ctx) == 0);
        CHECK(generated == i);
    }
    CHECK(swapcontext(&main_ctx, &gen_ctx) == 0);
    CHECK(generated == -1);

    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    sigprocmask(SIG_BLOCK, &blocked, NULL);
    resumed = 0;
    CHECK(getcontext(&uc) == 0);
    if (resumed++ == 0) {
        sigprocmask(SIG_UNBLOCK, &blocked, NULL);
        setcontext(&uc);
    }
    sigprocmask(SIG_BLOCK, NULL, &now);
    CHECK(sigismember(&now, SIGUSR1));
    sigprocmask(SIG_UNBLOCK, &blocked, NULL);

    fibre_main.env_init = 0;
    fibre_job.env_init = 0;
    new_stack(&fibre_job.fibre, NULL);
    makecontext(&fibre_job.fibre, job, 0);
    for (int i = 0; i < 4; i++)
        fibre_swap(&fibre_main, &fibre_job, 1);
    CHECK(job_steps == 4);

    memset(&uc, 0, sizeof(uc));
    makecontext(&uc, generator, 0);
    CHECK(uc.uc_mcsize == 0);
    errno = 0;
    CHECK(setcontext(&uc) == -1 && errno == EINVAL);

    CHECK(pipe(pipefd) == 0);
    pid = fork();
    if (pid == 0) {
        new_stack(&func_ctx, NULL);
        makecontext(&func_ctx, (void (*)(void))child_exits, 1, pipefd[1]);
        setcontext(&func_ctx);
        _exit(3);
    }
    close(pipefd[1]);
    CHECK(read(pipefd[0], buf, sizeof(buf) - 1) == 3 && strcmp(buf, "ran") == 0);
    CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);

    return checks_done("ucontext");
}
