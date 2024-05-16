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

#include <stdint.h>
#include <stdlib.h>
#include <ces/cesBus.h>
#include <ces/CesXpcBridge.h>

struct CesBus *cesGetBus(int a_a, int a_b, int a_c) { return NULL; }
void cesReleaseBus(struct CesBus *a_a) {}
int cesDmaElSetOption(int a_a, int a_b, int a_c) { return 0; }
char const *cesError2String(CesError a_a) { return NULL; }

struct CesXpcBridge *CesXpcBridge_GetByName(char const *a_a) { return NULL; }
void CesXpcBridge_Put(struct CesXpcBridge *a_a) {}

CesUint64 CesXpcBridge_MasterMap64(struct CesXpcBridge *a_a, CesUint64 a_b,
    CesUint32 a_c, CesUint32 a_d) { return 0; }
void CesXpcBridge_MasterUnMap64(struct CesXpcBridge *a_a, CesUint64 a_b,
    CesUint32 a_c) {}
void *CesXpcBridge_MasterMapVirt64(struct CesXpcBridge *a_a, CesUint64 a_b,
    CesUint32 a_c, CesUint32 a_d) { return NULL; }
void CesXpcBridge_MasterUnMapVirt(struct CesXpcBridge *a_a, void *a_b,
    CesUint32 a_c) {}
