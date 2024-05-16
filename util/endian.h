/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2020-2021, 2024
 * Bastian Löher
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

#ifndef UTIL_ENDIAN_H
#define UTIL_ENDIAN_H

#include <nconf/util/endian.h>

#if NCONF_mENDIAN_bBSD_BYTEORDER_H
/* NCONF_NOLINK */
#	include <bsd/bsd_byteorder.h>
#	if BYTE_ORDER == BIG_ENDIAN
#		define NURDLIB_BIG_ENDIAN 1
#	elif BYTE_ORDER == BIG_ENDIAN
#		define NURDLIB_LITTLE_ENDIAN 1
#	endif
#elif NCONF_mENDIAN_bENDIAN_H_DOUBLE
/* NCONF_NOLINK */
#	include <endian.h>
#	if !defined(__BYTE_ORDER)
#		error No __BYTE_ORDER.
#	elif __BYTE_ORDER == __BIG_ENDIAN
#		define NURDLIB_BIG_ENDIAN 1
#	elif __BYTE_ORDER == __LITTLE_ENDIAN
#		define NURDLIB_LITTLE_ENDIAN 1
#	endif
#elif NCONF_mENDIAN_bENDIAN_H_SINGLE
/* NCONF_NOLINK */
#	include <endian.h>
#	if !defined(_BYTE_ORDER)
#		error No _BYTE_ORDER.
#	elif _BYTE_ORDER == _BIG_ENDIAN
#		define NURDLIB_BIG_ENDIAN 1
#	elif _BYTE_ORDER == _LITTLE_ENDIAN
#		define NURDLIB_LITTLE_ENDIAN 1
#	endif
#elif NCONF_mENDIAN_bBSD_TCP_H
/* NCONF_NOLINK */
#	include <sys/types.h>
#	include <bsd/tcp.h>
#	if !defined(BYTE_ORDER)
#		error No BYTE_ORDER.
#	elif BYTE_ORDER == BIG_ENDIAN
#		define NURDLIB_BIG_ENDIAN 1
#	elif BYTE_ORDER == BIG_ENDIAN
#		define NURDLIB_LITTLE_ENDIAN 1
#	endif
#elif NCONF_mENDIAN_bDARWIN_MACHINE_ENDIAN_H
/* NCONF_NOLINK */
#	include <machine/endian.h>
#	if !defined(__DARWIN_BYTE_ORDER)
#		error No __DARWIN_BYTE_ORDER.
#	elif __DARWIN_BYTE_ORDER == __DARWIN_BIG_ENDIAN
#		define NURDLIB_BIG_ENDIAN 1
#	elif __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
#		define NURDLIB_LITTLE_ENDIAN 1
#	endif
#elif NCONF_mENDIAN_bFREEBSD_MACHINE_ENDIAN_H
/* NCONF_NOLINK */
#	include <machine/endian.h>
#	if !defined(_BYTE_ORDER)
#		error No _BYTE_ORDER.
#	elif _BYTE_ORDER == _BIG_ENDIAN
#		define NURDLIB_BIG_ENDIAN 1
#	elif _BYTE_ORDER == _LITTLE_ENDIAN
#		define NURDLIB_LITTLE_ENDIAN 1
#	endif
#endif

#if !defined(NURDLIB_BIG_ENDIAN) && !defined(NURDLIB_LITTLE_ENDIAN)
#	error "Endianness not set."
#endif
#if defined(NURDLIB_BIG_ENDIAN) && defined(NURDLIB_LITTLE_ENDIAN)
#	error "NURDLIB_BIG_ENDIAN and NURDLIB_LITTLE_ENDIAN both set."
#endif

#endif
