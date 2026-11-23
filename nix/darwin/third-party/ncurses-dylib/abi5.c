/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * The plain-named entry points of macOS's libncurses for callers built with a macOS 14 or older
 * SDK, whose cchar_t has no ext_color: each converts to the library's cchar_t and calls the
 * $NCURSES60 entry point that abi60.h names. The color pair of such a cchar_t lives in its
 * attributes, which is where ncurses reads it when ext_color is 0.
 */
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#define ACS_LEN 128

typedef struct {
	attr_t attr;
	wchar_t chars[CCHARW_MAX];
} cchar5_t;

_Static_assert(sizeof(cchar5_t) == 24, "macOS 14's cchar_t is 24 bytes");
_Static_assert(sizeof(cchar_t) == 28, "macOS 15's cchar_t is 28 bytes");

__thread int _nc_abiver = 5;

static cchar5_t wacs5[ACS_LEN];
void *kido_nc_wacs_public __asm("__nc_wacs") = NULL;

static const cchar_t *
to6(cchar_t *dst, const cchar5_t *src)
{
	if (src == NULL)
		return NULL;
	dst->attr = src->attr;
	memcpy(dst->chars, src->chars, sizeof(dst->chars));
	dst->ext_color = 0;
	return dst;
}

static void
to5(cchar5_t *dst, const cchar_t *src)
{
	dst->attr = src->attr;
	memcpy(dst->chars, src->chars, sizeof(dst->chars));
}

static void
publish_wacs(int abi)
{
	if (_nc_wacs == NULL || abi == 6) {
		kido_nc_wacs_public = _nc_wacs;
		return;
	}
	for (int i = 0; i < ACS_LEN; i++)
		to5(&wacs5[i], &_nc_wacs[i]);
	kido_nc_wacs_public = wacs5;
}

#define ABI5(name) __asm("_" #name)
#define ABI6(name) __asm("_" #name "$NCURSES60")

WINDOW *initscr5(void) ABI5(initscr);
WINDOW *initscr6(void) ABI6(initscr);
SCREEN *newterm5(const char *, FILE *, FILE *) ABI5(newterm);
SCREEN *newterm6(const char *, FILE *, FILE *) ABI6(newterm);

WINDOW *
initscr5(void)
{
	WINDOW *win = initscr();

	publish_wacs(5);
	return win;
}

WINDOW *
initscr6(void)
{
	WINDOW *win = initscr();

	publish_wacs(6);
	return win;
}

SCREEN *
newterm5(const char *name, FILE *ofp, FILE *ifp)
{
	SCREEN *sp = newterm(name, ofp, ifp);

	publish_wacs(5);
	return sp;
}

SCREEN *
newterm6(const char *name, FILE *ofp, FILE *ifp)
{
	SCREEN *sp = newterm(name, ofp, ifp);

	publish_wacs(6);
	return sp;
}

/* a cchar_t argument: CC5 declares the converted copy */
#define CC5(c) cchar_t c##6; const cchar_t *c##p = to6(&c##6, c)

int wadd_wch5(WINDOW *, const cchar5_t *) ABI5(wadd_wch);
int wins_wch5(WINDOW *, const cchar5_t *) ABI5(wins_wch);
int wecho_wchar5(WINDOW *, const cchar5_t *) ABI5(wecho_wchar);
int pecho_wchar5(WINDOW *, const cchar5_t *) ABI5(pecho_wchar);
int wbkgrnd5(WINDOW *, const cchar5_t *) ABI5(wbkgrnd);
void wbkgrndset5(WINDOW *, const cchar5_t *) ABI5(wbkgrndset);
int whline_set5(WINDOW *, const cchar5_t *, int) ABI5(whline_set);
int wvline_set5(WINDOW *, const cchar5_t *, int) ABI5(wvline_set);
int wborder_set5(WINDOW *, const cchar5_t *, const cchar5_t *, const cchar5_t *, const cchar5_t *,
    const cchar5_t *, const cchar5_t *, const cchar5_t *, const cchar5_t *) ABI5(wborder_set);
int box_set5(WINDOW *, const cchar5_t *, const cchar5_t *) ABI5(box_set);
wchar_t *wunctrl5(cchar5_t *) ABI5(wunctrl);
int getcchar5(const cchar5_t *, wchar_t *, attr_t *, NCURSES_PAIRS_T *, void *) ABI5(getcchar);
int setcchar5(cchar5_t *, const wchar_t *, const attr_t, NCURSES_PAIRS_T, const void *) ABI5(setcchar);
int win_wch5(WINDOW *, cchar5_t *) ABI5(win_wch);
int wgetbkgrnd5(WINDOW *, cchar5_t *) ABI5(wgetbkgrnd);
int wadd_wchnstr5(WINDOW *, const cchar5_t *, int) ABI5(wadd_wchnstr);
int win_wchnstr5(WINDOW *, cchar5_t *, int) ABI5(win_wchnstr);

int
wadd_wch5(WINDOW *win, const cchar5_t *c)
{
	CC5(c);
	return wadd_wch(win, cp);
}

int
wins_wch5(WINDOW *win, const cchar5_t *c)
{
	CC5(c);
	return wins_wch(win, cp);
}

int
wecho_wchar5(WINDOW *win, const cchar5_t *c)
{
	CC5(c);
	return wecho_wchar(win, cp);
}

int
pecho_wchar5(WINDOW *pad, const cchar5_t *c)
{
	CC5(c);
	return pecho_wchar(pad, cp);
}

int
wbkgrnd5(WINDOW *win, const cchar5_t *c)
{
	CC5(c);
	return wbkgrnd(win, cp);
}

void
wbkgrndset5(WINDOW *win, const cchar5_t *c)
{
	CC5(c);
	wbkgrndset(win, cp);
}

int
whline_set5(WINDOW *win, const cchar5_t *c, int n)
{
	CC5(c);
	return whline_set(win, cp, n);
}

int
wvline_set5(WINDOW *win, const cchar5_t *c, int n)
{
	CC5(c);
	return wvline_set(win, cp, n);
}

int
wborder_set5(WINDOW *win, const cchar5_t *ls, const cchar5_t *rs, const cchar5_t *ts,
    const cchar5_t *bs, const cchar5_t *tl, const cchar5_t *tr, const cchar5_t *bl,
    const cchar5_t *br)
{
	CC5(ls);
	CC5(rs);
	CC5(ts);
	CC5(bs);
	CC5(tl);
	CC5(tr);
	CC5(bl);
	CC5(br);
	return wborder_set(win, lsp, rsp, tsp, bsp, tlp, trp, blp, brp);
}

int
box_set5(WINDOW *win, const cchar5_t *v, const cchar5_t *h)
{
	CC5(v);
	CC5(h);
	return box_set(win, vp, hp);
}

wchar_t *
wunctrl5(cchar5_t *c)
{
	CC5(c);
	return wunctrl((cchar_t *)cp);
}

int
getcchar5(const cchar5_t *c, wchar_t *wch, attr_t *attrs, NCURSES_PAIRS_T *pair, void *opts)
{
	CC5(c);
	return getcchar(cp, wch, attrs, pair, opts);
}

int
setcchar5(cchar5_t *c, const wchar_t *wch, const attr_t attrs, NCURSES_PAIRS_T pair,
    const void *opts)
{
	cchar_t c6;
	int rc = setcchar(c != NULL ? &c6 : NULL, wch, attrs, pair, opts);

	if (rc == OK && c != NULL)
		to5(c, &c6);
	return rc;
}

int
win_wch5(WINDOW *win, cchar5_t *c)
{
	cchar_t c6;
	int rc = win_wch(win, c != NULL ? &c6 : NULL);

	if (rc == OK && c != NULL)
		to5(c, &c6);
	return rc;
}

int
wgetbkgrnd5(WINDOW *win, cchar5_t *c)
{
	cchar_t c6;
	int rc = wgetbkgrnd(win, c != NULL ? &c6 : NULL);

	if (rc == OK && c != NULL)
		to5(c, &c6);
	return rc;
}

int
wadd_wchnstr5(WINDOW *win, const cchar5_t *s, int n)
{
	cchar_t *s6;
	int len, rc;

	if (s == NULL)
		return wadd_wchnstr(win, NULL, n);
	for (len = 0; (n < 0 || len < n) && s[len].chars[0] != L'\0'; len++)
		;
	if ((s6 = calloc((size_t)len + 1, sizeof(*s6))) == NULL)
		return ERR;
	for (int i = 0; i < len; i++)
		to6(&s6[i], &s[i]);
	rc = wadd_wchnstr(win, s6, n);
	free(s6);
	return rc;
}

int
win_wchnstr5(WINDOW *win, cchar5_t *s, int n)
{
	static const cchar_t zero;
	cchar_t *s6;
	int limit, rc;

	if (win == NULL || s == NULL)
		return win_wchnstr(win, NULL, n);
	limit = getmaxx(win) - getcurx(win);
	if (n >= 0 && n < limit)
		limit = n;
	if ((s6 = calloc((size_t)limit + 1, sizeof(*s6))) == NULL)
		return ERR;
	rc = win_wchnstr(win, s6, n);
	if (rc == OK) {
		for (int i = 0; i <= limit; i++) {
			to5(&s[i], &s6[i]);
			if (memcmp(&s6[i], &zero, sizeof(zero)) == 0)
				break;
		}
	}
	free(s6);
	return rc;
}

void bkgrndset5(const cchar5_t *) ABI5(bkgrndset);

void
bkgrndset5(const cchar5_t *c)
{
	wbkgrndset5(stdscr, c);
}

/* the calls X/Open defines as macros over the ones above, as ncurses' lib_gen.c builds them */
#define DERIVED(ret, name, params, call) \
	ret name##5 params ABI5(name); \
	ret name##5 params { return call; }
#define AT(win, y, x, call) (wmove(win, y, x) == ERR ? ERR : (call))

DERIVED(int, add_wch, (const cchar5_t *c), wadd_wch5(stdscr, c))
DERIVED(int, add_wchnstr, (const cchar5_t *s, int n), wadd_wchnstr5(stdscr, s, n))
DERIVED(int, add_wchstr, (const cchar5_t *s), wadd_wchnstr5(stdscr, s, -1))
DERIVED(int, wadd_wchstr, (WINDOW *win, const cchar5_t *s), wadd_wchnstr5(win, s, -1))
DERIVED(int, mvadd_wch, (int y, int x, const cchar5_t *c), AT(stdscr, y, x, wadd_wch5(stdscr, c)))
DERIVED(int, mvadd_wchnstr, (int y, int x, const cchar5_t *s, int n),
    AT(stdscr, y, x, wadd_wchnstr5(stdscr, s, n)))
DERIVED(int, mvadd_wchstr, (int y, int x, const cchar5_t *s),
    AT(stdscr, y, x, wadd_wchnstr5(stdscr, s, -1)))
DERIVED(int, mvwadd_wch, (WINDOW *win, int y, int x, const cchar5_t *c), AT(win, y, x, wadd_wch5(win, c)))
DERIVED(int, mvwadd_wchnstr, (WINDOW *win, int y, int x, const cchar5_t *s, int n),
    AT(win, y, x, wadd_wchnstr5(win, s, n)))
DERIVED(int, mvwadd_wchstr, (WINDOW *win, int y, int x, const cchar5_t *s),
    AT(win, y, x, wadd_wchnstr5(win, s, -1)))
DERIVED(int, ins_wch, (const cchar5_t *c), wins_wch5(stdscr, c))
DERIVED(int, mvins_wch, (int y, int x, const cchar5_t *c), AT(stdscr, y, x, wins_wch5(stdscr, c)))
DERIVED(int, mvwins_wch, (WINDOW *win, int y, int x, const cchar5_t *c), AT(win, y, x, wins_wch5(win, c)))
DERIVED(int, echo_wchar, (const cchar5_t *c), wecho_wchar5(stdscr, c))
DERIVED(int, bkgrnd, (const cchar5_t *c), wbkgrnd5(stdscr, c))
DERIVED(int, getbkgrnd, (cchar5_t *c), wgetbkgrnd5(stdscr, c))
DERIVED(int, border_set, (const cchar5_t *ls, const cchar5_t *rs, const cchar5_t *ts,
    const cchar5_t *bs, const cchar5_t *tl, const cchar5_t *tr, const cchar5_t *bl, const cchar5_t *br),
    wborder_set5(stdscr, ls, rs, ts, bs, tl, tr, bl, br))
DERIVED(int, hline_set, (const cchar5_t *c, int n), whline_set5(stdscr, c, n))
DERIVED(int, mvhline_set, (int y, int x, const cchar5_t *c, int n),
    AT(stdscr, y, x, whline_set5(stdscr, c, n)))
DERIVED(int, mvwhline_set, (WINDOW *win, int y, int x, const cchar5_t *c, int n),
    AT(win, y, x, whline_set5(win, c, n)))
DERIVED(int, vline_set, (const cchar5_t *c, int n), wvline_set5(stdscr, c, n))
DERIVED(int, mvvline_set, (int y, int x, const cchar5_t *c, int n),
    AT(stdscr, y, x, wvline_set5(stdscr, c, n)))
DERIVED(int, mvwvline_set, (WINDOW *win, int y, int x, const cchar5_t *c, int n),
    AT(win, y, x, wvline_set5(win, c, n)))
DERIVED(int, in_wch, (cchar5_t *c), win_wch5(stdscr, c))
DERIVED(int, mvin_wch, (int y, int x, cchar5_t *c), AT(stdscr, y, x, win_wch5(stdscr, c)))
DERIVED(int, mvwin_wch, (WINDOW *win, int y, int x, cchar5_t *c), AT(win, y, x, win_wch5(win, c)))
DERIVED(int, in_wchnstr, (cchar5_t *s, int n), win_wchnstr5(stdscr, s, n))
DERIVED(int, in_wchstr, (cchar5_t *s), win_wchnstr5(stdscr, s, -1))
DERIVED(int, win_wchstr, (WINDOW *win, cchar5_t *s), win_wchnstr5(win, s, -1))
DERIVED(int, mvin_wchnstr, (int y, int x, cchar5_t *s, int n), AT(stdscr, y, x, win_wchnstr5(stdscr, s, n)))
DERIVED(int, mvin_wchstr, (int y, int x, cchar5_t *s), AT(stdscr, y, x, win_wchnstr5(stdscr, s, -1)))
DERIVED(int, mvwin_wchnstr, (WINDOW *win, int y, int x, cchar5_t *s, int n),
    AT(win, y, x, win_wchnstr5(win, s, n)))
DERIVED(int, mvwin_wchstr, (WINDOW *win, int y, int x, cchar5_t *s),
    AT(win, y, x, win_wchnstr5(win, s, -1)))
