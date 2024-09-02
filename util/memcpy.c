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

#include <nconf/util/memcpy.c>
#include <util/memcpy.h>

#if NCONF_mMEMCPY_bALTIVEC

/* NCONF_CFLAGS=-maltivec -mregnames */
void *
memcpy_(void *a_dst, void const *a_src, size_t a_bytes)
{
	return vec_memcpy(a_dst, a_src, a_bytes);
}

#elif NCONF_mMEMCPY_bPPC
#	ifndef __PPC
#		error "This is only nice for powerpc"
#	endif

void *
memcpy_(void *a_dst, void *a_src, size_t a_bytes)
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
#	include <string.h>

void *
memcpy_(void *a_dst, const void *a_src, size_t a_bytes)
{
#undef memcpy
	return memcpy(a_dst, a_src, a_bytes);
}

#elif NCONF_mMEMCPY_bCHARCOPY

void *
memcpy_(void *a_dst, void const *a_src, size_t a_bytes)
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

#if NCONFING_mMEMCPY
#	include <string.h>
#	define NCONF_TEST nconf_test_()
int nconf_test_(void);
int nconf_test_(void) {
	char const src[] = "abc";
	char dst[4];
	memcpy_(dst, src, sizeof src);
	return 0 == memcmp(dst, src, sizeof src);
}
#endif
