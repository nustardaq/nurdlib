/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2019, 2021-2022, 2024
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

#include <util/memcpy.h>

#if NCONF_mMEMCPY_bALTIVEC

void *
nurdlib_memcpy(void *a_dst, void const *a_src, size_t a_bytes)
{
	return vec_memcpy(a_dst, a_src, a_bytes);
}

#elif NCONF_mMEMCPY_bPPC

void *
nurdlib_memcpy(void *a_dst, void *a_src, size_t a_bytes)
{
	double *d_dst;
	double const *d_src;
	char *c_dst;
	char const *c_src, *c_end;
	size_t i;

	c_dst = a_dst;
	c_src = a_src;
	c_end = c_src + a_bytes % 8;
	while (c_src != c_end) {
		*c_dst++ = *c_src++;
	}

	d_dst = (void *)c_dst;
	d_src = (void const *)c_src;
	a_bytes /= 8;
	for (i = 0; i < a_bytes; i++) {
		*d_dst++ = *d_src++;
	}
	return a_dst;
}

#elif NCONF_mMEMCPY_bGLIBC

void *
nurdlib_memcpy(void *a_dst, const void *a_src, size_t a_bytes)
{
	return memcpy(a_dst, a_src, a_bytes);
}

#elif NCONF_mMEMCPY_bCHARCOPY

void *
nurdlib_memcpy(void *a_dst, void const *a_src, size_t a_bytes)
{
	char *c_dst;
	char const *c_src;
	size_t i;

	c_dst = a_dst;
	c_src = a_src;
	for (i = 0; i < a_bytes; i++) {
		*c_dst++ = *c_src++;
	}
	return a_dst;
}

#endif
