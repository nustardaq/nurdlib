/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021-2024
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

#ifndef MODULE_GSI_ETHERBONE_NCONF_H
#define MODULE_GSI_ETHERBONE_NCONF_H

#include <nconf/module/gsi_etherbone/nconf.h>

#if NCONF_mGSI_ETHERBONE_bYES_BSD_SOURCE
/* NCONF_CPPFLAGS=-I$WR_SYS/include */
/* NCONF_LIBS=$WR_SYS/lib/libetherbone.a */
/* NCONF_NOEXEC */
#	define _BSD_SOURCE 1
#	define NCONFING_EBONE 1
#elif NCONF_mGSI_ETHERBONE_bYES_DEFAULT_SOURCE
/* NCONF_CPPFLAGS=-I$WR_SYS/include */
/* NCONF_LIBS=$WR_SYS/lib/libetherbone.a */
/* NCONF_NOEXEC */
#	define _DEFAULT_SOURCE 1
#	define NCONFING_EBONE 1
#elif NCONF_mGSI_ETHERBONE_bNO
/* NCONF_NOLINK */
#endif

#if NCONFING_EBONE
#	include <etherbone.h>
#	include <stdlib.h>
#	if NCONFING_mGSI_ETHERBONE
#		define NCONF_TEST eb_status(0)
#	endif
#endif

#endif
