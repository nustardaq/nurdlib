/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2019, 2023-2024
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

#ifndef MODULE_SIS_3801_INTERNAL_H
#define MODULE_SIS_3801_INTERNAL_H

#include <module/module.h>

struct Sis3801Module {
	struct	Module module;
	uint32_t	address;
	uint32_t	channel_enable;
	struct	Map *sicy_map;
	/*
	 * The first trigger _starts_ the scaling, and subsequent triggers
	 * will give the scaling from the previous trigger, i.e. the first
	 * trigger will give no data.
	 */
	int	is_running;
};

#endif
