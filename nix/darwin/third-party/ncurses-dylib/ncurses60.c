/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Symbols macOS's libncurses (ncurses 6.0) exports that ncurses dropped since: two obsoleted in
 * 2004 and removed in 2017, as ncurses 6.0 defined them, and the program name tic.h declares.
 */
#include <curses.priv.h>
#include <tic.h>

const char *_nc_progname;

void (*_nc_check_termtype)(TERMTYPE *);

int
_nc_resolve_uses(bool fullresolve)
{
	return _nc_resolve_uses2(fullresolve, FALSE);
}
