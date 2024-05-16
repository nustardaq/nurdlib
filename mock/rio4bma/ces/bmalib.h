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

#ifndef MOCK_CES_BMALIB_H
#define MOCK_CES_BMALIB_H

#include <stdint.h>
#include <stdlib.h>

enum {
	BMA_DEFAULT_MODE,

	BMA_M_AmCode,
	BMA_M_VMESize,
	BMA_M_WordSize,
	BMA_M_VMEAdrInc,

	BMA_M_AmA32U,
	BMA_M_WzD32,
	BMA_M_MbltU,
	BMA_M_WzD64,

	BMA_M_VaiFifo,
	BMA_M_VaiNormal
};

void	bma_close(void);
int	bma_open(void);
void	bma_perror(char const *, int);
int	bma_read_count(uintptr_t, uintptr_t, size_t, int);
int	bma_set_mode(int, int, int);

#endif
