/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019, 2023-2024
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

#include <module/gsi_vulom/gsi_vulom.h>
#include <module/trloii/module.h>
#include <nurdlib/trloii.h>

#define NAME "Gsi Vulom"

TRLOII_MODULE_GENERIC(vulom)
#if NCONF_mVULOM4_bYES
#	include <hwmap/hwmap_mapvme.h>
#	include <include/trlo_access.h>
#	include <nurdlib/serialio.h>
TRLOII_MODULE(VULOM, vulom, TRLO, trlo, LEMO)
#else
TRLOII_MODULE_EMPTY(VULOM, vulom)
#endif
