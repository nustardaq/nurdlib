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

#ifndef MOCK_XPC326_DMA_H
#define MOCK_XPC326_DMA_H

enum {
	XPC_DMA_CHAN_MODIFY,
	XPC_DMA_EXEC,
	XPC_DMA_READ,
	XPC_DMA_USER_BUFFER,
	XPC_DMA_VME_ADDR_CONST,
	XPC_DMA_VME_BURST_512,
	XPC_DMA_VME_BURST_1024
};

typedef struct {
	uintptr_t	mem_address;
	uintptr_t	xpc_address;
	size_t	size;
	int	options;
} xpc_dma_t;

#endif
