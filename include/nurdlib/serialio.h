/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017, 2020-2022, 2024
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

#ifndef MODULE_SERIALIO_H
#define MODULE_SERIALIO_H

#include <nconf/include/nurdlib/serialio.h>

#if NCONF_mSERIALIO_bEIEIO
/* NCONF_NOLINK */
#	define SERIALIZE_IO __asm__ volatile ("eieio")
#elif NCONF_mSERIALIO_bMFENCE
/* NCONF_NOLINK */
#	define SERIALIZE_IO __asm__ volatile ("mfence" ::: "memory")
#elif NCONF_mSERIALIO_bDMB
/* NCONF_NOLINK */
#	define SERIALIZE_IO __asm__ volatile ("dmb")
#elif NCONF_mSERIALIO_bSILLY
/* NCONF_NOLINK */
#	define SERIALIZE_IO __asm__ volatile ("" ::: "memory")
#endif
#if NCONFING_mSERIALIO
#	define NCONF_TEST_VOID SERIALIZE_IO
#endif

#endif
