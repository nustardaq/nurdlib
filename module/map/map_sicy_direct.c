/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2021, 2023-2024
 * Bastian Löher
 * Håkan T Johansson
 * Michael Munch
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

#include <module/map/internal.h>
#include <nurdlib/log.h>

#ifdef SICY_DIRECT

MAP_FUNC_EMPTY(sicy_deinit);

void
sicy_map(struct Map *a_map)
{
	a_map->private = (void*)(uintptr_t)
	    (SICY_DIRECT_A32_BASE + a_map->address);
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	uint8_t const *p8;

	p8 = a_map->private;
	return *(uint16_t const *)(p8 + a_ofs);
}

uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	uint8_t const *p8;

	p8 = a_map->private;
	return *(uint32_t const *)(p8 + a_ofs);
}

MAP_FUNC_EMPTY(sicy_setup);
MAP_FUNC_EMPTY(sicy_shutdown);
UNMAP_FUNC_EMPTY(sicy);

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	uint8_t *p8;

	p8 = a_map->private;
	*(uint16_t *)(p8 + a_ofs) = a_u16;
}

void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	uint8_t *p8;

	p8 = a_map->private;
	*(uint32_t *)(p8 + a_ofs) = a_u32;
}

#endif
