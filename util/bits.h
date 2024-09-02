/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2018, 2021, 2024
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

#ifndef UTIL_BITS_H
#define UTIL_BITS_H

#include <util/funcattr.h>
#include <util/stdint.h>

/*
 * "Safe" top mask generator, top=0..31 is the index of the top-most bit:
 *  0 -> 0x1
 *  1 -> 0x3
 * 15 -> 0xffff
 * 31 -> 0xffffffff
 *
 * The "unsafe" version would be ((1 << 32) - 1) which goes out of range for a
 * 32-bit all-on mask. The C standard says wrapping with unsigned is defined,
 * but some compilers still don't likey.
 */
#define BITS_MASK_TOP(top) (((1UL << (top)) - 1UL) | (1UL << (top)))
#define BITS_MASK(bot, top) (BITS_MASK_TOP(top) & ~(BITS_MASK_TOP(bot) >> 1))
#define BITS_GET(i, lsb, msb) (BITS_MASK_TOP(msb - lsb) & (i >> lsb))

int	bits_get_count(uint32_t) FUNC_PURE FUNC_RETURNS;

#ifdef htonl
#	undef htonl
#endif
#define htonl PLEASE_USE_htonl_
uint32_t	htonl_(uint32_t);

#endif
