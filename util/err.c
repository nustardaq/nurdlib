/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2024
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

#include <util/err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void	base(int, char const *, va_list) FUNC_PRINTF(2, 0);
static void	printer(char const *, ...);
static int	stdio(char const *, va_list) FUNC_PRINTF(1, 0);

static ErrPrinter g_printer = stdio;

void
base(int a_errno, char const *a_fmt, va_list a_args)
{
	g_printer(a_fmt, a_args);
	printer(": %s\n", strerror(a_errno));
}

void
err_set_printer(ErrPrinter a_printer)
{
	g_printer = NULL == a_printer ? stdio : a_printer;
}

void
err_(int a_exit_code, char const *a_fmt, ...)
{
	va_list args;
	int errno_ = errno;

	va_start(args, a_fmt);
	base(errno_, a_fmt, args);
	va_end(args);
	exit(a_exit_code);
}

void
errx_(int a_exit_code, char const *a_fmt, ...)
{
	va_list args;

	va_start(args, a_fmt);
	g_printer(a_fmt, args);
	va_end(args);
	exit(a_exit_code);
}

void
printer(char const *a_fmt, ...)
{
	va_list args;

	va_start(args, a_fmt);
	g_printer(a_fmt, args);
	va_end(args);
}

int
stdio(char const *a_fmt, va_list args)
{
	return vfprintf(stderr, a_fmt, args);
}

void
warn_(char const *a_fmt, ...)
{
	va_list args;
	int errno_ = errno;

	va_start(args, a_fmt);
	base(errno_, a_fmt, args);
	va_end(args);
}

void
warnx_(char const *a_fmt, ...)
{
	va_list args;

	va_start(args, a_fmt);
	g_printer(a_fmt, args);
	va_end(args);
}

void
vwarnx_(char const *a_fmt, va_list a_args)
{
	g_printer(a_fmt, a_args);
}
