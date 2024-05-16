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

#ifndef MOCK_XPC3310_CESXPCBRIDGE_H
#define MOCK_XPC3310_CESXPCBRIDGE_H

#include <ces/cesBus.h>

struct CesXpcBridge {
	int	a;
};

struct CesXpcBridge *CesXpcBridge_GetByName(char const *);
void CesXpcBridge_Put(struct CesXpcBridge *);

CesUint64 CesXpcBridge_MasterMap64(struct CesXpcBridge *, CesUint64,
    CesUint32, CesUint32);
void CesXpcBridge_MasterUnMap64(struct CesXpcBridge *, CesUint64, CesUint32);
void *CesXpcBridge_MasterMapVirt64(struct CesXpcBridge *, CesUint64,
    CesUint32, CesUint32);
void CesXpcBridge_MasterUnMapVirt(struct CesXpcBridge *, void *, CesUint32);

#endif
