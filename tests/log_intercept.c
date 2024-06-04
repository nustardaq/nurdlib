/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2017, 2019, 2021, 2023-2024
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

#include <tests/log_intercept.h>
#include <nurdlib/base.h>
#include <util/err.h>
#include <util/string.h>

static struct {
	unsigned	level;
	char	str[128];
} g_log[1000];
static size_t	g_num;

void
log_intercept(char const *a_file, int a_line_no, unsigned a_level, char const
    *a_str)
{
	(void)a_file;
	(void)a_line_no;
	if (LENGTH(g_log) > g_num) {
		g_log[g_num].level = a_level;
		strlcpy(g_log[g_num].str, a_str, sizeof g_log[g_num].str);
		++g_num;
	}
}

void
log_intercept_clear(void)
{
	g_num = 0;
}

unsigned
log_intercept_get_level(size_t a_idx)
{
	if (g_num <= a_idx) {
		errx_(EXIT_FAILURE, "g_num <= a_idx.");
	}
	return g_log[a_idx].level;
}

char const *
log_intercept_get_str(size_t a_idx)
{
	if (g_num <= a_idx) {
		errx_(EXIT_FAILURE, "g_num <= a_idx.");
	}
	return g_log[a_idx].str;
}

size_t
log_intercept_get_num(void)
{
	return g_num;
}
