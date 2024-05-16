/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2017, 2021, 2024
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
#include <util/endian.h>
#include <util/stdint.h>

NTEST(DucksAndGluttony)
{
	union {
		uint32_t	u32;
		uint8_t	u8[4];
	} u;

	u.u32 = 0x12345678;
#if defined(NURDLIB_BIG_ENDIAN)
	NTRY_I(0x12, ==, u.u8[0]);
	NTRY_I(0x34, ==, u.u8[1]);
	NTRY_I(0x56, ==, u.u8[2]);
	NTRY_I(0x78, ==, u.u8[3]);
#elif defined(NURDLIB_LITTLE_ENDIAN)
	NTRY_I(0x78, ==, u.u8[0]);
	NTRY_I(0x56, ==, u.u8[1]);
	NTRY_I(0x34, ==, u.u8[2]);
	NTRY_I(0x12, ==, u.u8[3]);
#else
#	error NURDLIB_ENDIAN not nconf:ed properly.
#endif
}

NTEST_SUITE(Endianness)
{
	NTEST_ADD(DucksAndGluttony);
}
