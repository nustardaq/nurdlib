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

#include <ntest/ntest.h>

static int const c_zero = 0;
static int const c_one = 1;

NTEST(ThisShouldFail)
{
	NTRY_BOOL(0 != 0);
	NTRY_BOOL(0 == 1);
	NTRY_I(c_zero + 1, ==, c_zero);
	NTRY_I(c_zero, !=, c_one - 1);
}

NTEST_SUITE(Fail)
{
	NTEST_ADD(ThisShouldFail);
}
