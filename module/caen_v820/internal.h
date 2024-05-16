/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2017, 2019-2020, 2023-2024
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

#ifndef MODULE_CAEN_V820_INTERNAL_H
#define MODULE_CAEN_V820_INTERNAL_H

#include <module/caen_v8n0/internal.h>

struct CaenV820Module {
	struct	CaenV8n0Module v8n0;
	uint32_t	channel_mask;
	int	channel_num;
};

#endif
