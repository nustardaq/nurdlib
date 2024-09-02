/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024
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

#include <nconf/util/fs.c>

#if NCONF_mFTRUNCATE_bNONE
/* NCONF_NOEXEC */
#elif NCONF_mFTRUNCATE_bBSD_SOURCE
/* NCONF_NOEXEC */
#	define _BSD_SOURCE 1
#elif NCONF_mFTRUNCATE_bDEFAULT_SOURCE
/* NCONF_NOEXEC */
#	define _DEFAULT_SOURCE 1
#endif
#if NCONFING_mFTRUNCATE
#	define NCONF_TEST ftruncate(0, 0)
#endif

#include <util/fs.h>

#undef ftruncate

int
ftruncate_(int a_fildes, off_t a_len)
{
	return ftruncate(a_fildes, a_len);
}
