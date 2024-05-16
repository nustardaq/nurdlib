/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
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

#ifndef MOCK_XPC3310_CESBUS_H
#define MOCK_XPC3310_CESBUS_H

enum {
	CES_BUS_VIRT,
	CES_BUS_XPC,
	CES_DMAEL_FIFO_MODE,

	CES_EINVAL,
	CES_EALIGN,
	CES_EINTERNAL,
	CES_OK
};

typedef uint32_t CesUint32;
typedef int64_t CesInt64;
typedef uint64_t CesUint64;

typedef int CesError;

struct CesBus {
	int	a;
};

struct CesBus *cesGetBus(int, int, int);
void cesReleaseBus(struct CesBus *);
int cesDmaElSetOption(int, int, int);
char const *cesError2String(CesError);

#endif
