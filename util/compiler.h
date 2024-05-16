/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
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

#ifndef UTIL_COMPILER_H
#define UTIL_COMPILER_H

#include <nconf/util/compiler.h>

#if NCONF_mWEXTRA_bWEXTRA
/* NCONF_CFLAGS=-Wextra */
/* NCONF_NOLINK */
#elif NCONF_mWEXTRA_bW
/* NCONF_CFLAGS=-W */
/* NCONF_NOLINK */
#endif

#if NCONF_mWSTRICT_ALIASING_bYES
/* NCONF_CFLAGS=-fstrict-aliasing -Wstrict-aliasing */
/* NCONF_NOLINK */
#elif NCONF_mWSTRICT_ALIASING_bNO
/* NCONF_NOLINK */
#endif

#if NCONF_mWSTRICT_OVERFLOW_bYES
/* NCONF_CFLAGS=-Wstrict-overflow */
/* NCONF_NOLINK */
#elif NCONF_mWSTRICT_OVERFLOW_bNO
/* NCONF_NOLINK */
#endif

#if NCONF_mQUNUSED_ARGUMENTS_bYES
/* NCONF_CFLAGS=-Qunused-arguments */
/* NCONF_NOLINK */
#elif NCONF_mQUNUSED_ARGUMENTS_bNO
/* NCONF_NOLINK */
#endif

#endif
