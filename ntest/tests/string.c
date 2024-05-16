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

NTEST(SimpleStrings)
{
	char const c_hello[] = "Hello";

	NTRY_STR("", ==, "");
	NTRY_STR("Hello", ==, c_hello);
	NTRY_STR("abc", <, "def");
	NTRY_STR("def", >, "abc");
	/* GCC break optimized version of comparing 0-length strings. */
	NTRY_STRN("abc", ==, "abd", 1);
	NTRY_STRN("abc", ==, "abd", 2);
	NTRY_STRN("abc", !=, "abd", 3);
}

NTEST(NullStringsFail)
{
	NTRY_STR(NULL, ==, NULL);
	NTRY_STR(NULL, ==, "urg");
	NTRY_STR("urg", ==, NULL);
}

NTEST_SUITE(String)
{
	NTEST_ADD(SimpleStrings);
	NTEST_ADD(NullStringsFail);
}
