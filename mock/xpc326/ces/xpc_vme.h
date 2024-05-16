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

#ifndef MOCK_XPC326_VME_H
#define MOCK_XPC326_VME_H

enum {
	XPC_MASTER_MAP,
	XPC_MASTER_UNMAP,
	XPC_VME_A32_STD_USER,
	XPC_VME_A32_BLT_USER,
	XPC_VME_A32_MBLT_USER,
	XPC_VME_ATYPE_A32,
	XPC_VME_DTYPE_2eSST,
	XPC_VME_PTYPE_USER,
	XPC_VME_FREQ_SST320,
	XPC_VME_SAFE_PIO
};

typedef int xpc_dma_opts_e;

typedef struct {
	uintptr_t	vme_address;
	int	access_type;
	int	write_access;
	size_t	data_size;
} xpc_vme_safe_pio_t;

typedef struct {
	uintptr_t	bus_address;
	size_t	size;
	int	access_type;
	uintptr_t	xpc_address;
} xpc_master_map_t;

#endif
