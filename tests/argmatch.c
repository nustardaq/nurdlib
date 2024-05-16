/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2021, 2024
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
#include <nurdlib/base.h>
#include <util/argmatch.h>

NTEST(Shorts)
{
	char *c_argv[] = {"boh", "-i", "-ij", "a"};

	g_argind = 0;
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'j', NULL, NULL));
	NTRY_I(1, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, 'i', NULL, NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'j', NULL, NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, 'i', NULL, NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'i', NULL, NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, 'j', NULL, NULL));
	NTRY_I(3, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'a', NULL, NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'i', NULL, NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'j', NULL, NULL));
}

NTEST(Longs)
{
	char *c_argv[] = {"boh", "--i", "--j", "--kl"};

	g_argind = 0;
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "j", NULL));
	NTRY_I(1, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, -1, "i", NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "i", NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, -1, "j", NULL));
	NTRY_I(3, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "k", NULL));
	NTRY_I(3, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "l", NULL));
	NTRY_I(3, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, -1, "kl", NULL));
	NTRY_I(LENGTH(c_argv), ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "i", NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "j", NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "k", NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "l", NULL));
}

NTEST(Combos)
{
	char *c_argv[] = {"boh", "-i", "--j"};

	g_argind = 0;
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "i", NULL));
	NTRY_I(1, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, 'i', NULL, NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'j', NULL, NULL));
	NTRY_I(2, ==, g_argind);
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, -1, "j", NULL));
	NTRY_I(3, ==, g_argind);
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'i', NULL, NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, 'j', NULL, NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "i", NULL));
	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "j", NULL));
}

NTEST(ShortsWithValues)
{
	char *c_argv[] = {"boh", "-i", "1", "-j2", "-k=3"};
	char const *value;

	g_argind = 0;
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, 'i', NULL, &value));
	NTRY_STR("1", ==, value);
	NTRY_I(3, ==, g_argind);

	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, 'j', NULL, &value));
	NTRY_STR("2", ==, value);
	NTRY_I(4, ==, g_argind);

	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, 'k', NULL, &value));
	NTRY_STR("3", ==, value);
	NTRY_I(5, ==, g_argind);
}

NTEST(LongsWithValues)
{
	char *c_argv[] = {"boh", "--i", "1", "--j=2", "--k3"};
	char const *value;

	g_argind = 0;
	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, -1, "i", &value));
	NTRY_STR("1", ==, value);
	NTRY_I(3, ==, g_argind);

	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, -1, "j", &value));
	NTRY_STR("2", ==, value);
	NTRY_I(4, ==, g_argind);

	NTRY_BOOL(!arg_match(LENGTH(c_argv), c_argv, -1, "k", &value));
	NTRY_I(4, ==, g_argind);

	NTRY_BOOL(arg_match(LENGTH(c_argv), c_argv, -1, "k3", NULL));
	NTRY_I(5, ==, g_argind);
}

NTEST(MissingValue)
{
	char *c_argvs[] = {"boh", "-i"};
	char *c_argvl[] = {"boh", "--i"};
	char const *value;

	g_argind = 0;
	NTRY_BOOL(!arg_match(LENGTH(c_argvs), c_argvs, 'i', NULL, &value));
	NTRY_I(1, ==, g_argind);
	g_argind = 0;
	NTRY_BOOL(!arg_match(LENGTH(c_argvl), c_argvl, -1, "i", &value));
	NTRY_I(1, ==, g_argind);
}

NTEST_SUITE(ArgMatch)
{
	NTEST_ADD(Shorts);
	NTEST_ADD(Longs);
	NTEST_ADD(Combos);
	NTEST_ADD(ShortsWithValues);
	NTEST_ADD(LongsWithValues);
	NTEST_ADD(MissingValue);
}
