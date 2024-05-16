/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2018, 2021, 2024
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

#include <util/argmatch.h>
#include <string.h>

#include <stdio.h>

int g_argind = 0;
static int g_argsubind;

int
arg_match(int a_argc, char **a_argv, char a_short, char const *a_long, char
    const **a_value)
{
	char const *arg;
	int long_len;

	if (0 == g_argind) {
		g_argind = 1;
		g_argsubind = 0;
	}
	if (a_argc <= g_argind) {
		return 0;
	}

	arg = a_argv[g_argind];
	long_len = NULL == a_long ? 0 : strlen(a_long);

	if (0 == g_argsubind) {
		if ('-' != arg[0]) {
			return 0;
		}
		if ('-' == arg[1]) {
			if (NULL == a_long) {
				return 0;
			}
			if (0 != strncmp(arg + 2, a_long, long_len)) {
				return 0;
			}
			if ('\0' != arg[2 + long_len] &&
			    '=' != arg[2 + long_len]) {
				return 0;
			}
			++g_argind;
			if (NULL == a_value) {
				return 1;
			}
			if ('=' == arg[2 + long_len]) {
				*a_value = arg + 2 + long_len + 1;
				return 1;
			}
			if (a_argc == g_argind) {
				--g_argind;
				return 0;
			}
			*a_value = a_argv[g_argind];
			++g_argind;
			return 1;
		}
	}
	if (a_short == arg[1 + g_argsubind]) {
		++g_argsubind;
		if (NULL == a_value) {
			if ('\0' == arg[1 + g_argsubind]) {
				++g_argind;
				g_argsubind = 0;
			}
			return 1;
		}
		if ('\0' != arg[1 + g_argsubind]) {
			++g_argsubind;
			if ('=' == arg[g_argsubind]) {
				++g_argsubind;
			}
			*a_value = arg + g_argsubind;
			++g_argind;
			g_argsubind = 0;
			return 1;
		}
		++g_argind;
		if (a_argc == g_argind) {
			--g_argind;
			return 0;
		}
		*a_value = a_argv[g_argind];
		++g_argind;
		g_argsubind = 0;
		return 1;
	}
	return 0;
}
