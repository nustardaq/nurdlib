/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2024
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

#include <util/bits.h>
#include <util/endian.h>

static int const c_bitlut[16] = {
	0, 1, 1, 2, 1, 2, 2, 3,
	1, 2, 2, 3, 2, 3, 3, 4
};

int
bits_get_count(uint32_t a_value)
{
	int count, i;

	count = 0;
	for (i = 0; 32 > i; i += 4) {
		count += c_bitlut[0xf & (a_value >> i)];
	}
	return count;
}

uint32_t
htonl_(uint32_t a_u32)
{
#if NURDLIB_BIG_ENDIAN
	return a_u32;
#else
	return
	    (a_u32 & 0xff000000) >> 24 |
	    (a_u32 & 0x00ff0000) >>  8 |
	    (a_u32 & 0x0000ff00) <<  8 |
	    (a_u32 & 0x000000ff) << 24;
#endif
}
