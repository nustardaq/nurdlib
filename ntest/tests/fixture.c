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

static int *g_mojo;

NTEST(SimpleFixture1)
{
	NTRY_U(*g_mojo, ==, 0x12345678);
	++(*g_mojo);
	NTRY_U(*g_mojo, ==, 0x12345679);
}

NTEST(SimpleFixture2)
{
	NTRY_U(*g_mojo, ==, 0x12345678);
}

NTEST_SUITE(SimpleFixture)
{
	g_mojo = malloc(sizeof *g_mojo);
	*g_mojo = 0x12345678;

	NTEST_ADD(SimpleFixture1);
	NTEST_ADD(SimpleFixture2);

	free(g_mojo);
}
