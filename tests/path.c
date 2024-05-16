/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2021, 2024
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
#include <string.h>
#include <nurdlib/base.h>
#include <util/path.h>

NTEST(basename_dup)
{
	char *p;

	p = basename_dup("");
	NTRY_STR("", ==, p);
	FREE(p);

	p = basename_dup("file.ext");
	NTRY_STR("file.ext", ==, p);
	FREE(p);

	p = basename_dup("./file.ext");
	NTRY_STR("file.ext", ==, p);
	FREE(p);

	p = basename_dup("../file.ext");
	NTRY_STR("file.ext", ==, p);
	FREE(p);

	p = basename_dup("bla/file.ext");
	NTRY_STR("file.ext", ==, p);
	FREE(p);

	p = basename_dup("bla/");
	NTRY_STR("", ==, p);
	FREE(p);

	p = basename_dup("/bla/file.ext");
	NTRY_STR("file.ext", ==, p);
	FREE(p);
}

NTEST(dirname_dup)
{
	char *p;

	p = dirname_dup("");
	NTRY_STR(".", ==, p);
	FREE(p);

	p = dirname_dup("file.ext");
	NTRY_STR(".", ==, p);
	FREE(p);

	p = dirname_dup("./file.ext");
	NTRY_STR(".", ==, p);
	FREE(p);

	p = dirname_dup("../file.ext");
	NTRY_STR("..", ==, p);
	FREE(p);

	p = dirname_dup("bla/file.ext");
	NTRY_STR("bla", ==, p);
	FREE(p);

	p = dirname_dup("bla/");
	NTRY_STR("bla", ==, p);
	FREE(p);

	p = dirname_dup("/bla/file.ext");
	NTRY_STR("/bla", ==, p);
	FREE(p);
}

NTEST_SUITE(UtilPath)
{
	NTEST_ADD(basename_dup);
	NTEST_ADD(dirname_dup);
}
