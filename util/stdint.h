/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2024-2025
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

#ifndef UTIL_STDINT_H
#define UTIL_STDINT_H

#include <nconf/util/stdint.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

#if NCONF_mSTDINT_LIMITS_bSTD_H
/* NCONF_NOLINK */
#elif NCONF_mSTDINT_LIMITS_bLAST_RESORT
/* NCONF_NOLINK */
#	undef SIZE_MAX
#	undef INT32_MIN
#	undef INT32_MAX
#	undef UINT32_MAX
#	undef INT64_MIN
#	undef INT64_MAX
#	undef UINT64_MAX
#	define INT32_MIN -2147483648L
#	define INT32_MAX 2147483647L
#	define UINT32_MAX 4294967295UL
#	define INT64_MIN -9223372036854775808LL
#	define INT64_MAX 9223372036854775807LL
#	define UINT64_MAX 18446744073709551615ULL
#	if __WORDSIZE == 64
#		define SIZE_MAX UINT64_MAX
#	else
#		define SIZE_MAX UINT32_MAX
#	endif
#endif
#if NCONFING_mSTDINT_LIMITS
#	define NCONF_TEST SIZE_MAX
#endif

#if NCONF_mSTDINT_TYPES_bSTD_H
/* NCONF_NOLINK */
#elif NCONF_mSTDINT_TYPES_bTYPEDEF_SYS_TYPES_H
/* NCONF_NOLINK */
typedef u_int8_t  uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
typedef u_int64_t uint64_t;
typedef int32_t   intptr_t;
typedef uint32_t  uintptr_t;
#endif
#if NCONFING_mSTDINT_TYPES
#	define NCONF_TEST nconf_test_()
static size_t nconf_test_(void)
{
	char pi8 [             1 ==    sizeof(int8_t) ? 1 : -1];
	char pu8 [             1 ==   sizeof(uint8_t) ? 1 : -1];
	char pi16[             2 ==   sizeof(int16_t) ? 1 : -1];
	char pu16[             2 ==  sizeof(uint16_t) ? 1 : -1];
	char pi32[             4 ==   sizeof(int32_t) ? 1 : -1];
	char pu32[             4 ==  sizeof(uint32_t) ? 1 : -1];
	char pi64[             8 ==   sizeof(int64_t) ? 1 : -1];
	char pu64[             8 ==  sizeof(uint64_t) ? 1 : -1];
	char pip [sizeof(void *) ==  sizeof(intptr_t) ? 1 : -1];
	char pup [sizeof(void *) == sizeof(uintptr_t) ? 1 : -1];
	return
	    !!sizeof(pi8) *
	    !!sizeof(pu8) *
	    !!sizeof(pi16) *
	    !!sizeof(pu16) *
	    !!sizeof(pi32) *
	    !!sizeof(pu32) *
	    !!sizeof(pi64) *
	    !!sizeof(pu64) *
	    !!sizeof(pip) *
	    !!sizeof(pup);
}
#endif

#endif
