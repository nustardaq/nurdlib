/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2021, 2023-2024
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

#ifndef MODULE_SIS_3820_SCALER_INTERNAL_H
#define MODULE_SIS_3820_SCALER_INTERNAL_H

#include <module/module.h>

struct Sis3820ScalerModule {
	struct	Module module;
	uint32_t	address;
	enum Keyword	blt_mode;
	int	is_latching;
	struct	Map *sicy_map;
	struct	Map *dma_map;
	unsigned	data_format;
	uint32_t	channel_mask;
	unsigned	channel_num;
	size_t	channel_map[32];
};

#endif
