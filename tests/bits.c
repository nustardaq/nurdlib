/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016, 2018, 2021, 2024
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
#include <util/bits.h>

NTEST(Gets)
{
	NTRY_U(0xffffffff, ==, BITS_GET(0xffffffff,  0, 31));
	NTRY_U(0x00000001, ==, BITS_GET(0xffffffff,  0,  0));
	NTRY_U(0x00000001, ==, BITS_GET(0xffffffff, 31, 31));
	NTRY_U(0x00000007, ==, BITS_GET(0x81234567,  0,  3));
	NTRY_U(0x00000006, ==, BITS_GET(0x81234567,  4,  7));
	NTRY_U(0x00000005, ==, BITS_GET(0x81234567,  8, 11));
	NTRY_U(0x00000004, ==, BITS_GET(0x81234567, 12, 15));
	NTRY_U(0x00000003, ==, BITS_GET(0x81234567, 16, 19));
	NTRY_U(0x00000002, ==, BITS_GET(0x81234567, 20, 23));
	NTRY_U(0x00000001, ==, BITS_GET(0x81234567, 24, 27));
	NTRY_U(0x00000008, ==, BITS_GET(0x81234567, 28, 31));
	NTRY_U(0x091a2b3c, ==, BITS_GET(0x12345678,  1, 30));
}

NTEST(Masks)
{
	NTRY_U(0x00000001, ==, BITS_MASK_TOP( 0));
	NTRY_U(0x00000003, ==, BITS_MASK_TOP( 1));
	NTRY_U(0x0000ffff, ==, BITS_MASK_TOP(15));
	NTRY_U(0xffffffff, ==, BITS_MASK_TOP(31));

	NTRY_U(0x00000001, ==, BITS_MASK( 0,  0));
	NTRY_U(0x00000003, ==, BITS_MASK( 0,  1));
	NTRY_U(0x00000007, ==, BITS_MASK( 0,  2));
	NTRY_U(0x00000006, ==, BITS_MASK( 1,  2));
	NTRY_U(0x0000ffff, ==, BITS_MASK( 0, 15));
	NTRY_U(0x7fff8000, ==, BITS_MASK(15, 30));
	NTRY_U(0xffff0000, ==, BITS_MASK(16, 31));
	NTRY_U(0xffffffff, ==, BITS_MASK( 0, 31));
}

NTEST(CountLoops)
{
	int i;

	for (i = 0; 32 > i; ++i) {
		NTRY_I(1, ==, bits_get_count(1UL << i));
	}
	for (i = 0; 32 > i; ++i) {
		NTRY_I(i, ==, bits_get_count((1UL << i) - 1));
	}
	for (i = 0; 32 > i; ++i) {
		NTRY_I(i / 2, ==, bits_get_count(
		    0xaaaaaaaa & ((1UL << i) - 1)));
	}
}

NTEST_SUITE(UtilBits)
{
	NTEST_ADD(Gets);
	NTEST_ADD(Masks);
	NTEST_ADD(CountLoops);
}
