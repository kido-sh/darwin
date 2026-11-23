/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Appended to ncurses' curses.wide for the build. macOS's libncurses has two entry points for each
 * call that passes a cchar_t: NAME for callers built with a macOS 14 or older SDK, whose cchar_t
 * has no ext_color, and NAME$NCURSES60 for later SDKs. The library is built with the later layout,
 * so its own definitions take the $NCURSES60 names and abi5.c supplies the plain ones.
 */
#if NCURSES_WIDECHAR

#define KIDO_ABI60(name) __asm("_" #name "$NCURSES60")

extern NCURSES_EXPORT(int) (add_wch) (const cchar_t *) KIDO_ABI60(add_wch);
extern NCURSES_EXPORT(int) (add_wchnstr) (const cchar_t *, int) KIDO_ABI60(add_wchnstr);
extern NCURSES_EXPORT(int) (add_wchstr) (const cchar_t *) KIDO_ABI60(add_wchstr);
extern NCURSES_EXPORT(int) (bkgrnd) (const cchar_t *) KIDO_ABI60(bkgrnd);
extern NCURSES_EXPORT(void) (bkgrndset) (const cchar_t *) KIDO_ABI60(bkgrndset);
extern NCURSES_EXPORT(int) (border_set) (const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*) KIDO_ABI60(border_set);
extern NCURSES_EXPORT(int) (box_set) (WINDOW *, const cchar_t *, const cchar_t *) KIDO_ABI60(box_set);
extern NCURSES_EXPORT(int) (echo_wchar) (const cchar_t *) KIDO_ABI60(echo_wchar);
extern NCURSES_EXPORT(int) (getbkgrnd) (cchar_t *) KIDO_ABI60(getbkgrnd);
extern NCURSES_EXPORT(int) (getcchar) (const cchar_t *, wchar_t*, attr_t*, NCURSES_PAIRS_T*, void*) KIDO_ABI60(getcchar);
extern NCURSES_EXPORT(int) (hline_set) (const cchar_t *, int) KIDO_ABI60(hline_set);
extern NCURSES_EXPORT(int) (in_wch) (cchar_t *) KIDO_ABI60(in_wch);
extern NCURSES_EXPORT(int) (in_wchnstr) (cchar_t *, int) KIDO_ABI60(in_wchnstr);
extern NCURSES_EXPORT(int) (in_wchstr) (cchar_t *) KIDO_ABI60(in_wchstr);
extern NCURSES_EXPORT(int) (ins_wch) (const cchar_t *) KIDO_ABI60(ins_wch);
extern NCURSES_EXPORT(int) (mvadd_wch) (int, int, const cchar_t *) KIDO_ABI60(mvadd_wch);
extern NCURSES_EXPORT(int) (mvadd_wchnstr) (int, int, const cchar_t *, int) KIDO_ABI60(mvadd_wchnstr);
extern NCURSES_EXPORT(int) (mvadd_wchstr) (int, int, const cchar_t *) KIDO_ABI60(mvadd_wchstr);
extern NCURSES_EXPORT(int) (mvhline_set) (int, int, const cchar_t *, int) KIDO_ABI60(mvhline_set);
extern NCURSES_EXPORT(int) (mvin_wch) (int, int, cchar_t *) KIDO_ABI60(mvin_wch);
extern NCURSES_EXPORT(int) (mvin_wchnstr) (int, int, cchar_t *, int) KIDO_ABI60(mvin_wchnstr);
extern NCURSES_EXPORT(int) (mvin_wchstr) (int, int, cchar_t *) KIDO_ABI60(mvin_wchstr);
extern NCURSES_EXPORT(int) (mvins_wch) (int, int, const cchar_t *) KIDO_ABI60(mvins_wch);
extern NCURSES_EXPORT(int) (mvvline_set) (int, int, const cchar_t *, int) KIDO_ABI60(mvvline_set);
extern NCURSES_EXPORT(int) (mvwadd_wch) (WINDOW *, int, int, const cchar_t *) KIDO_ABI60(mvwadd_wch);
extern NCURSES_EXPORT(int) (mvwadd_wchnstr) (WINDOW *, int, int, const cchar_t *, int) KIDO_ABI60(mvwadd_wchnstr);
extern NCURSES_EXPORT(int) (mvwadd_wchstr) (WINDOW *, int, int, const cchar_t *) KIDO_ABI60(mvwadd_wchstr);
extern NCURSES_EXPORT(int) (mvwhline_set) (WINDOW *, int, int, const cchar_t *, int) KIDO_ABI60(mvwhline_set);
extern NCURSES_EXPORT(int) (mvwin_wch) (WINDOW *, int, int, cchar_t *) KIDO_ABI60(mvwin_wch);
extern NCURSES_EXPORT(int) (mvwin_wchnstr) (WINDOW *, int,int, cchar_t *,int) KIDO_ABI60(mvwin_wchnstr);
extern NCURSES_EXPORT(int) (mvwin_wchstr) (WINDOW *, int, int, cchar_t *) KIDO_ABI60(mvwin_wchstr);
extern NCURSES_EXPORT(int) (mvwins_wch) (WINDOW *, int, int, const cchar_t *) KIDO_ABI60(mvwins_wch);
extern NCURSES_EXPORT(int) (mvwvline_set) (WINDOW *, int,int, const cchar_t *,int) KIDO_ABI60(mvwvline_set);
extern NCURSES_EXPORT(int) (pecho_wchar) (WINDOW *, const cchar_t *) KIDO_ABI60(pecho_wchar);
extern NCURSES_EXPORT(int) (setcchar) (cchar_t *, const wchar_t *, const attr_t, NCURSES_PAIRS_T, const void *) KIDO_ABI60(setcchar);
extern NCURSES_EXPORT(int) (vline_set) (const cchar_t *, int) KIDO_ABI60(vline_set);
extern NCURSES_EXPORT(int) (wadd_wch) (WINDOW *,const cchar_t *) KIDO_ABI60(wadd_wch);
extern NCURSES_EXPORT(int) (wadd_wchnstr) (WINDOW *,const cchar_t *,int) KIDO_ABI60(wadd_wchnstr);
extern NCURSES_EXPORT(int) (wadd_wchstr) (WINDOW *,const cchar_t *) KIDO_ABI60(wadd_wchstr);
extern NCURSES_EXPORT(int) (wbkgrnd) (WINDOW *,const cchar_t *) KIDO_ABI60(wbkgrnd);
extern NCURSES_EXPORT(void) (wbkgrndset) (WINDOW *,const cchar_t *) KIDO_ABI60(wbkgrndset);
extern NCURSES_EXPORT(int) (wborder_set) (WINDOW *,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*,const cchar_t*) KIDO_ABI60(wborder_set);
extern NCURSES_EXPORT(int) (wecho_wchar) (WINDOW *, const cchar_t *) KIDO_ABI60(wecho_wchar);
extern NCURSES_EXPORT(int) (wgetbkgrnd) (WINDOW *, cchar_t *) KIDO_ABI60(wgetbkgrnd);
extern NCURSES_EXPORT(int) (whline_set) (WINDOW *, const cchar_t *, int) KIDO_ABI60(whline_set);
extern NCURSES_EXPORT(int) (win_wch) (WINDOW *, cchar_t *) KIDO_ABI60(win_wch);
extern NCURSES_EXPORT(int) (win_wchnstr) (WINDOW *, cchar_t *, int) KIDO_ABI60(win_wchnstr);
extern NCURSES_EXPORT(int) (win_wchstr) (WINDOW *, cchar_t *) KIDO_ABI60(win_wchstr);
extern NCURSES_EXPORT(int) (wins_wch) (WINDOW *, const cchar_t *) KIDO_ABI60(wins_wch);
extern NCURSES_EXPORT(wchar_t*) (wunctrl) (cchar_t *) KIDO_ABI60(wunctrl);
extern NCURSES_EXPORT(int) (wvline_set) (WINDOW *, const cchar_t *, int) KIDO_ABI60(wvline_set);

/* abi5.c publishes _nc_wacs in the caller's layout when a screen is set up */
extern NCURSES_EXPORT_VAR(cchar_t *) _nc_wacs __asm("_kido_nc_wacs");
extern NCURSES_EXPORT(WINDOW *) (initscr) (void) __asm("_kido_nc_initscr");
extern NCURSES_EXPORT(SCREEN *) (newterm) (const char *,FILE *,FILE *) __asm("_kido_nc_newterm");

#endif /* NCURSES_WIDECHAR */
