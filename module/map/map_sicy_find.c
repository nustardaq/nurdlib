/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2021, 2023-2024
 * Michael Munch
 * Stephane Pietri
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
#include <util/fmtmod.h>

#ifdef SICY_FINDCONTROLLER

#	include <sys/types.h>
#	include <ces/vmelib.h>

intptr_t	find_controller(uintptr_t, size_t, unsigned, unsigned,
    unsigned, struct pdparam_master const *);
void		return_controller(intptr_t, size_t);

MAP_FUNC_EMPTY(sicy_deinit);

void
sicy_map(struct Map *a_map)
{
	struct pdparam_master param;
	intptr_t addr;

	LOGF(verbose)(LOGL, "sicy_map {");
	param.iack = 1;
	param.rdpref = 0;
	param.wrpost = 0;
	param.swap = 0;
	param.dum[0] = 0;
	addr = find_controller(a_map->address, a_map->bytes, 9, 0, 0, &param);
	if (-1 == addr) {
		log_die(LOGL, "Could not find controller "
		    "(addr=0x%08x,bytes=0x%08"PRIzx").", a_map->address,
		    a_map->bytes);
	}
	a_map->private = (void *)addr;
	LOGF(verbose)(LOGL, "sicy_map(addr=%p) }", a_map->private);
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

void
sicy_unmap(struct Map *a_map)
{
	LOGF(verbose)(LOGL, "sicy_unmap {");
	return_controller(a_map->address, a_map->bytes);
	LOGF(verbose)(LOGL, "sicy_unmap }");
}

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
