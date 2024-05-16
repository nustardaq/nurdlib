/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2019, 2021-2024
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

#ifndef UTIL_MEMCPY_H
#define UTIL_MEMCPY_H

#include <nconf/util/memcpy.h>
#include <stdlib.h>

void *nurdlib_memcpy(void *, void const *, size_t);

#if NCONF_mMEMCPY_bALTIVEC
/* NCONF_CFLAGS=-maltivec -mregnames */
/* NCONF_SRC=util/libmotovec_memcpy.S util/memcpy.c */
void *vec_memcpy(void *, void const *, size_t);
#elif NCONF_mMEMCPY_bPPC
/* NCONF_SRC=util/memcpy.c */
#	ifndef __PPC
#		error "This is only nice for powerpc"
#	endif
#elif NCONF_mMEMCPY_bGLIBC
/* NCONF_SRC=util/memcpy.c */
#	include <string.h>
#elif NCONF_mMEMCPY_bCHARCOPY
/* NCONF_SRC=util/memcpy.c */
#endif

#if NCONFING_mMEMCPY
#	include <string.h>
#	define NCONF_TEST nconf_test_()
int nconf_test_(void);
int nconf_test_(void) {
	char const src[] = "abc";
	char dst[4];
	nurdlib_memcpy(dst, src, sizeof src);
	return 0 == memcmp(dst, src, sizeof src);
}
#endif

#endif
