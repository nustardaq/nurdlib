/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2018, 2021, 2024
 * Bastian Löher
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
#include <nurdlib/keyword.h>
#include <nurdlib/log.h>

NTEST(Lookup)
{
	unsigned i;

	for (i = 1; i < KW_THIS_IS_ALWAYS_LAST; ++i) {
		char const *str;
		enum Keyword id;

		str = keyword_get_string(i);
		id = keyword_get_id(str);
		NTRY_I(id, ==, i);
	}
}

NTEST(StupidNames)
{
	enum Keyword dummy;

	(void)dummy;
	NTRY_SIGNAL(dummy = keyword_get_id("tjosanhejsan"));
	NTRY_SIGNAL(dummy = keyword_get_id("ThisAintAKeyword"));
}

NTEST(StupidIDs)
{
	char const *str;

	(void)str;
	NTRY_SIGNAL(str = keyword_get_string(KW_NONE - 1));
	NTRY_SIGNAL(str = keyword_get_string(KW_THIS_IS_ALWAYS_LAST));
}

NTEST_SUITE(Keyword)
{
	NTEST_ADD(Lookup);
	NTEST_ADD(StupidNames);
	NTEST_ADD(StupidIDs);
}
