/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#define NCURSES_WIDECHAR 1

#include <curses.h>
#include <fcntl.h>
#include <form.h>
#include <locale.h>
#include <menu.h>
#include <panel.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <term.h>
#include <unistd.h>
#include <util.h>
#include <wchar.h>
#include "../libsystem/check.h"

/* the cchar_t of the macOS 14 SDK, for the plain-named entry points older binaries call */
typedef struct {
    attr_t attr;
    wchar_t chars[CCHARW_MAX];
} cchar5_t;

int setcchar5(cchar5_t *, const wchar_t *, attr_t, short, const void *) __asm("_setcchar");
int getcchar5(const cchar5_t *, wchar_t *, attr_t *, short *, void *) __asm("_getcchar");
int wadd_wch5(WINDOW *, const cchar5_t *) __asm("_wadd_wch");
int win_wch5(WINDOW *, cchar5_t *) __asm("_win_wch");
int wadd_wchnstr5(WINDOW *, const cchar5_t *, int) __asm("_wadd_wchnstr");
int win_wchnstr5(WINDOW *, cchar5_t *, int) __asm("_win_wchnstr");
WINDOW *initscr5(void) __asm("_initscr");

static FILE *results;
static int child_run, child_failed;

#define CCHECK(cond) do { \
        child_run++; \
        if (!(cond)) { \
            child_failed++; \
            fprintf(results, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

static void
mark(const char *s)
{
    mvaddstr(LINES - 1, 0, s);
    refresh();
}

static void
curses_session(void)
{
    cchar_t c, r;
    cchar5_t c5, r5, arr5[4], out5[8];
    wchar_t wch[CCHARW_MAX + 1];
    attr_t attrs;
    short pair;
    wchar_t hline;
    WINDOW *w;
    PANEL *p;
    ITEM *items[3];
    MENU *menu;
    FIELD *fields[2];
    FORM *form;
    SCREEN *mine, *other;
    FILE *devnull;
    int k1, k2;

    CCHECK(initscr() == stdscr && stdscr != NULL);
    CCHECK(LINES == 24 && COLS == 80);
    CCHECK(has_colors() && start_color() == OK && COLORS == 256);
    CCHECK(init_pair(1, COLOR_RED, COLOR_BLACK) == OK);
    attron(COLOR_PAIR(1));
    mvaddstr(0, 0, "kido-curses");
    attroff(COLOR_PAIR(1));

    CCHECK(setcchar(&c, L"K", A_BOLD, 1, NULL) == OK);
    CCHECK(mvadd_wch(1, 0, &c) == OK);
    CCHECK(mvin_wch(1, 0, &r) == OK && getcchar(&r, wch, &attrs, &pair, NULL) == OK);
    CCHECK(wch[0] == L'K' && (attrs & A_BOLD) && pair == 1);

    CCHECK(setcchar5(&c5, L"Q", A_UNDERLINE, 1, NULL) == OK);
    move(2, 0);
    CCHECK(wadd_wch5(stdscr, &c5) == OK);
    move(2, 0);
    CCHECK(win_wch5(stdscr, &r5) == OK && r5.chars[0] == L'Q');
    CCHECK((r5.attr & A_UNDERLINE) && PAIR_NUMBER(r5.attr) == 1);
    CCHECK(getcchar5(&r5, wch, &attrs, &pair, NULL) == OK && wch[0] == L'Q' && pair == 1);
    CCHECK(mvin_wch(2, 0, &r) == OK && getcchar(&r, wch, &attrs, &pair, NULL) == OK);
    CCHECK(wch[0] == L'Q' && (attrs & A_UNDERLINE) && pair == 1);

    setcchar5(&arr5[0], L"X", A_NORMAL, 0, NULL);
    setcchar5(&arr5[1], L"Y", A_REVERSE, 0, NULL);
    setcchar5(&arr5[2], L"Z", A_NORMAL, 1, NULL);
    memset(&arr5[3], 0, sizeof(arr5[3]));
    move(3, 0);
    CCHECK(wadd_wchnstr5(stdscr, arr5, -1) == OK);
    memset(out5, 0xff, sizeof(out5));
    move(3, 0);
    CCHECK(win_wchnstr5(stdscr, out5, 3) == OK);
    CCHECK(out5[0].chars[0] == L'X' && out5[1].chars[0] == L'Y' && out5[2].chars[0] == L'Z');
    CCHECK((out5[1].attr & A_REVERSE) && PAIR_NUMBER(out5[2].attr) == 1 && out5[3].chars[0] == 0);
    CCHECK(mvin_wch(3, 2, &r) == OK && getcchar(&r, wch, &attrs, &pair, NULL) == OK && wch[0] == L'Z');

    hline = WACS_HLINE->chars[0];
    CCHECK(hline == 0x2500 || (hline == L'q' && (WACS_HLINE->attr & A_ALTCHARSET)));
    CCHECK(initscr5() == stdscr);
    CCHECK(((cchar5_t *)_nc_wacs)['q'].chars[0] == hline);
    CCHECK(initscr() == stdscr && WACS_HLINE->chars[0] == hline);
    CCHECK(mvhline_set(4, 0, WACS_HLINE, 10) == OK);
    CCHECK(mvin_wch(4, 5, &r) == OK && r.chars[0] == hline);

    w = newwin(4, 20, 6, 10);
    p = new_panel(w);
    box(w, 0, 0);
    mvwaddstr(w, 1, 1, "kido-panel");
    update_panels();
    CCHECK(panel_window(p) == w && panel_below(NULL) == p);
    CCHECK(hide_panel(p) == OK && panel_hidden(p) == TRUE);
    CCHECK(show_panel(p) == OK && panel_hidden(p) == FALSE);
    doupdate();

    items[0] = new_item("alpha", "first");
    items[1] = new_item("beta", "second");
    items[2] = NULL;
    menu = new_menu(items);
    CCHECK(menu != NULL && post_menu(menu) == E_OK);
    CCHECK(menu_driver(menu, REQ_DOWN_ITEM) == E_OK && item_index(current_item(menu)) == 1);
    CCHECK(strcmp(item_name(current_item(menu)), "beta") == 0);
    unpost_menu(menu);
    free_menu(menu);
    free_item(items[0]);
    free_item(items[1]);

    fields[0] = new_field(1, 10, 12, 0, 0, 0);
    fields[1] = NULL;
    form = new_form(fields);
    CCHECK(form != NULL && post_form(form) == E_OK);
    form_driver(form, 'h');
    form_driver(form, 'i');
    CCHECK(form_driver_w(form, OK, L'!') == E_OK);
    CCHECK(form_driver(form, REQ_VALIDATION) == E_OK);
    CCHECK(strncmp(field_buffer(fields[0], 0), "hi!", 3) == 0);
    unpost_form(form);
    free_form(form);
    free_field(fields[0]);

    devnull = fopen("/dev/null", "r+");
    mine = set_term(NULL);
    set_term(mine);
    other = newterm("vt100", devnull, devnull);
    CCHECK(other != NULL && strcmp(termname(), "vt100") == 0);
    CCHECK(set_term(mine) == other && strcmp(termname(), "xterm-256color") == 0);
    delscreen(other);
    fclose(devnull);

    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    timeout(10000);
    mark("kido-ready");
    k1 = getch();
    k2 = getch();
    CCHECK(k1 == KEY_UP && k2 == 'x');
    endwin();
}

static int
run_pty(void)
{
    char buf[65536];
    size_t len = 0;
    ssize_t n;
    int master, pfd[2], status, typed = 0;
    struct winsize ws = { .ws_row = 24, .ws_col = 80 };
    pid_t pid;

    pipe(pfd);
    pid = forkpty(&master, NULL, NULL, &ws);
    if (pid == 0) {
        close(pfd[0]);
        results = fdopen(pfd[1], "w");
        setenv("TERM", "xterm-256color", 1);
        unsetenv("LINES");
        unsetenv("COLUMNS");
        if (setlocale(LC_CTYPE, "UTF-8") == NULL)
            setlocale(LC_CTYPE, "");
        curses_session();
        fprintf(results, "%d %d\n", child_run, child_failed);
        fclose(results);
        _exit(0);
    }
    close(pfd[1]);
    while (poll(&(struct pollfd){ .fd = master, .events = POLLIN }, 1, 20000) == 1 &&
           (n = read(master, buf + len, sizeof(buf) - 1 - len)) > 0) {
        len += n;
        buf[len] = '\0';
        if (!typed && memmem(buf, len, "kido-ready", 10) != NULL) {
            write(master, "\033OAx", 4);
            typed = 1;
        }
    }
    if (!typed)
        kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(typed);
    CHECK(memmem(buf, len, "\033[?1049h", 8) != NULL && memmem(buf, len, "\033[?1049l", 8) != NULL);
    CHECK(memmem(buf, len, "\033[31m", 5) != NULL || memmem(buf, len, "\033[38;5;1m", 9) != NULL);
    CHECK(memmem(buf, len, "kido-curses", 11) != NULL && memmem(buf, len, "kido-panel", 10) != NULL);
    {
        FILE *in = fdopen(pfd[0], "r");
        char line[256];
        int run = 0, failed = 0;

        while (fgets(line, sizeof(line), in) != NULL) {
            if (strncmp(line, "FAIL", 4) == 0)
                fputs(line, stdout);
            else
                sscanf(line, "%d %d", &run, &failed);
        }
        fclose(in);
        CHECK(run > 0);
        checks_run += run;
        checks_failed += failed;
    }
    return 0;
}

int
main(void)
{
    char tbuf[2048], area[2048], *ap = area, *s;
    char *names[] = { "xterm", "xterm-256color", "screen", "screen-256color", "tmux-256color",
        "vt100", "vt220", "linux", "ansi", "dumb", "rxvt", "alacritty", "kitty", "ghostty", "foot",
        "wezterm", "nsterm", "Apple_Terminal", "putty", "konsole", NULL };
    int err, i;

    alarm(120);
    CHECK(strncmp(curses_version(), "ncurses 6.", 10) == 0);
    for (i = 0; names[i] != NULL; i++) {
        int ok = setupterm(names[i], open("/dev/null", O_WRONLY), &err) == OK && err == 1;

        if (!ok)
            printf("FAIL no terminfo entry for %s\n", names[i]);
        CHECK(ok);
    }
    CHECK(setupterm("xterm-256color", open("/dev/null", O_WRONLY), &err) == OK);
    CHECK(tigetnum("colors") == 256 && tigetflag("am") == 1);
    s = tigetstr("smcup");
    CHECK(s != NULL && s != (char *)-1 && strncmp(s, "\033[?1049h", 8) == 0);
    s = tparm(tigetstr("cup"), 4, 9);
    CHECK(s != NULL && strcmp(s, "\033[5;10H") == 0);
    i = setupterm("kido-no-such-terminal", open("/dev/null", O_WRONLY), &err);
    if (i != ERR || err > 0)
        printf("setupterm of an unknown terminal: %d, err %d\n", i, err);
    CHECK(i == ERR && err <= 0);
    CHECK(tgetent(tbuf, "vt100") == 1 && tgetnum("co") == 80);
    s = tgetstr("cl", &ap);
    CHECK(s != NULL && strstr(s, "\033[H\033[J") != NULL);
    CHECK(strncmp(tgoto(tgetstr("cm", &ap), 9, 4), "\033[5;10H", 7) == 0);

    run_pty();
    return checks_done("curses");
}
