/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020, 2023-2024
 * Hans Toshihide Törnqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <util/sigbus.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <nurdlib/log.h>

static void (*g_sigbus_handler_orig)(int);
static int g_sigbus_is_ok;
static int g_sigbus_was;

static void
sigbus_handler(int a_signum)
{
	if (SIGBUS != a_signum || !g_sigbus_is_ok) {
		g_sigbus_handler_orig(a_signum);
	} else {
		g_sigbus_was = 1;
	}
}

void
sigbus_set_ok(int a_yes)
{
	g_sigbus_is_ok = a_yes;
	g_sigbus_was = 0;
}

void
sigbus_setup(void)
{
	if (!g_sigbus_handler_orig) {
		g_sigbus_handler_orig = signal(SIGBUS, sigbus_handler);
/*		if (SIG_ERR == g_sigbus_handler_orig) {
			log_die(LOGL, "signal: %s", strerror(errno));
		}*/
	}
}

int
sigbus_was(void)
{
	return g_sigbus_was;
}
