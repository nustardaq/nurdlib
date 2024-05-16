/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019, 2021-2022, 2024
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

#include <ntest/ntest.h>
#include <nurdlib.h>
#include <stdio.h>
#include <util/assert.h>
#include <util/math.h>
#include <util/ssort.h>
#include <util/string.h>

static int	is_sorted(uint16_t const *, size_t);

int
is_sorted(uint16_t const *a_array, size_t a_num)
{
	size_t i;
	uint16_t prev;

	prev = a_array[0];
	for (i = 1; a_num > i; ++i) {
		if (a_array[i] < prev) {
			return 0;
		}
		prev = a_array[i];
	}
	return 1;
}

NTEST(Sizes)
{
	NTRY_I(2, ==, sizeof(uint16_t));
	NTRY_I(4, ==, sizeof(uint32_t));
	NTRY_I(sizeof(void *), ==, sizeof(intptr_t));
}

NTEST(Rounding)
{
	NTRY_I(-1, ==, i32_round_double(-1.0));
	NTRY_I(-1, ==, i32_round_double(-0.5));
	NTRY_I(0, ==, i32_round_double(-0.4999));
	NTRY_I(0, ==, i32_round_double(0.0));
	NTRY_I(0, ==, i32_round_double(0.4999));
	NTRY_I(1, ==, i32_round_double(0.5));
	NTRY_I(1, ==, i32_round_double(1.0));
}

NTEST(ShellSort)
{
	uint16_t buf[128];
	int i, n;

	buf[0] = 0;
	buf[1] = 1;
	buf[2] = 2;
	buf[3] = 2;
	buf[4] = 1;
	NTRY_BOOL(is_sorted(buf, 4));
	NTRY_BOOL(!is_sorted(buf + 1, 4));

	n = sizeof buf / sizeof buf[0];
	for (i = 0; 10 > i; ++i) {
		int j;

		for (j = 0; n > j; ++j) {
			buf[j] = 0xaaaaaaaa ^ j;
		}
		NTRY_BOOL(!is_sorted(buf, n));
		shell_sort(buf, n);
		NTRY_BOOL(is_sorted(buf, n));
	}
}

NTEST(strtoi32)
{
	NTRY_I(0, ==, strtoi32("0", NULL, 10));
	NTRY_I(0, ==, strtoi32("0", NULL, 16));

	NTRY_I(1, ==, strtoi32("1", NULL, 10));
	NTRY_I(1, ==, strtoi32("1", NULL, 16));

	NTRY_I(10, ==, strtoi32("10", NULL, 10));
	NTRY_I(16, ==, strtoi32("10", NULL, 16));

	NTRY_I(2147483647, ==, strtoi32("2147483647", NULL, 10));
	NTRY_I(0x7fffffff, ==, strtoi32("7fffffff", NULL, 16));

	NTRY_I(2147483648U, ==, strtoi32("2147483648", NULL, 10));
	NTRY_I(0x80000000, ==, strtoi32("80000000", NULL, 16));

	NTRY_I(-1, ==, strtoi32("4294967295", NULL, 10));
	NTRY_I(-1, ==, strtoi32("ffffffff", NULL, 16));
}

#ifndef NDEBUG
NTEST(Assert)
{
	int i;

	i = 0;
	ASSERT(int, "d", 0, ==, i);
	++i;
	NTRY_SIGNAL(ASSERT(int, "d", 0, ==, i));
	STATIC_ASSERT(1);
	/* Cannot fail static_assert, this won't build -_- */
}
#endif

NTEST_SUITE(Util)
{
	NTEST_ADD(Sizes);
	NTEST_ADD(Rounding);
	NTEST_ADD(ShellSort);
	NTEST_ADD(strtoi32);
#ifndef NDEBUG
	NTEST_ADD(Assert);
#endif
}
