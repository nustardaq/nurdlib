/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018, 2021, 2024
 * Michael Munch
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
#include <util/memcpy.h>
#include <string.h>

NTEST(Copy1Bytes)
{
	char src[1], dst[1];

	memset(src, 0xb, sizeof(src));
	memset(dst, 0x0, sizeof(dst));

	NTRY_I(0xb, ==, *((char*)memcpy_(dst, src, sizeof(src))));
}

NTEST(Copy2Bytes)
{
	char src[2], dst[2];

	memset(src, 0xb, sizeof(src));
	memset(dst, 0x0, sizeof(dst));

	memcpy_(dst, src, sizeof(src));

	NTRY_I(0xb, ==, dst[0]);
	NTRY_I(0xb, ==, dst[1]);
}

NTEST(Copy3Bytes)
{
	char src[3], dst[3];

	memset(src, 0xb, sizeof(src));
	memset(dst, 0x0, sizeof(dst));

	memcpy_(dst, src, sizeof(src));

	NTRY_I(0xb, ==, dst[0]);
	NTRY_I(0xb, ==, dst[1]);
	NTRY_I(0xb, ==, dst[2]);
}

NTEST(Copy5DifferentBytes)
{
	char dst[5];
	char src[] = {1, 2, 3, 4, 5};
	memset(dst, 0x0, sizeof(dst));

	memcpy_(dst, src, sizeof(src));

	NTRY_I(0x1, ==, dst[0]);
	NTRY_I(0x2, ==, dst[1]);
	NTRY_I(0x3, ==, dst[2]);
	NTRY_I(0x4, ==, dst[3]);
	NTRY_I(0x5, ==, dst[4]);
}

NTEST(Copy0xa3Bytes)
{
	char src[3], dst[3];

	memset(src, 0xa, sizeof(src));
	memset(dst, 0x0, sizeof(dst));

	memcpy_(dst, src, sizeof(src));

	NTRY_I(0xa, ==, dst[0]);
	NTRY_I(0xa, ==, dst[1]);
	NTRY_I(0xa, ==, dst[2]);
}

NTEST_SUITE(MemCpy)
{
	NTEST_ADD(Copy1Bytes);
	NTEST_ADD(Copy2Bytes);
	NTEST_ADD(Copy3Bytes);
	NTEST_ADD(Copy0xa3Bytes);
	NTEST_ADD(Copy5DifferentBytes);
}
