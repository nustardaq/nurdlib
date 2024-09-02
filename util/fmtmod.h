/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2024
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

#ifndef UTIL_FMTMOD_H
#define UTIL_FMTMOD_H

#include <nconf/util/fmtmod.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/stdint.h>

#if NCONF_mFMTMOD_SIZET_bUINT
/* NCONF_NOLINK */
#	define PRIz "u"
#	define PRIzx "x"
#elif NCONF_mFMTMOD_SIZET_bZ
/* NCONF_NOLINK */
#	define PRIz "z"
#	define PRIzx "zx"
#elif NCONF_mFMTMOD_SIZET_bULONG
/* NCONF_NOLINK */
#	define PRIz "lu"
#	define PRIzx "lx"
#endif
#if NCONFING_mFMTMOD_SIZET
#	define NCONF_TEST \
    printf("%"PRIz" %"PRIzx, (size_t)argc, (size_t)argc)
#endif

#if NCONF_mFMTMOD_UINTPTRT_bUINT
/* NCONF_NOLINK */
#	define PRIp "u"
#	define PRIpx "x"
#elif NCONF_mFMTMOD_UINTPTRT_bULONG
/* NCONF_NOLINK */
#	define PRIp "lu"
#	define PRIpx "lx"
#endif
#if NCONFING_mFMTMOD_UINTPTRT
#	define NCONF_TEST \
    printf("%"PRIp" %"PRIpx, (uintptr_t)argc, (uintptr_t)argc)
#endif

#endif
