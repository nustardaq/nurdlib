/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2021, 2023-2024
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
#include <nurdlib/base.h>
#include <nurdlib/log.h>

#ifdef SICY_SMEM

#	include <errno.h>
#	include <smem.h>
#	include <stdio.h>

struct SiCy {
	char	*name;
	uintptr_t	page_ofs;
	uintptr_t	phys_addr;
	void	*ptr;
};

MAP_FUNC_EMPTY(sicy_deinit);

void
sicy_map(struct Map *a_map)
{
	size_t len;
	uint32_t page_start;
	char *p;
	struct SiCy *private;

	LOGF(verbose)(LOGL, "sicy_map {");

	CALLOC(private, 1);

	len = 8 + 1 + 8 + 1;
	MALLOC(private->name, len);
	snprintf(private->name, len, "%08x_%"PRIzx, a_map->address,
	    a_map->bytes);
	page_start = 0xfffff000 & a_map->address;
	private->page_ofs = 0x00000fff & a_map->address;
	private->phys_addr = SICY_SMEM_A32_BASE + page_start;
	p = smem_create(private->name, (void *)private->phys_addr,
	    a_map->bytes, SM_READ | SM_WRITE);
	if (NULL == p) {
		log_die(LOGL, "Could not map shared memory segment "
		    "(addr=0x%"PRIpx", size=0x%08"PRIzx"): %s.",
		    private->phys_addr, a_map->bytes, strerror(errno));
	}
	private->ptr = p + private->page_ofs;
	a_map->private = private;

	LOGF(verbose)(LOGL, "sicy_map(page=%p, ptr=%p) }",
	    (void *)p, private->ptr);
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	struct SiCy *private;
	uint8_t const *p8;

	private = a_map->private;
	p8 = private->ptr;
	return *(uint16_t const *)(p8 + a_ofs);
}

uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	struct SiCy *private;
	uint8_t const *p8;

	private = a_map->private;
	p8 = private->ptr;
	return *(uint32_t const *)(p8 + a_ofs);
}

MAP_FUNC_EMPTY(sicy_setup);
MAP_FUNC_EMPTY(sicy_shutdown);

void
sicy_unmap(struct Map *a_map)
{
	struct SiCy *private;
	char *p;

	LOGF(verbose)(LOGL, "sicy_unmap {");

	private = a_map->private;
	if (!private) {
		log_die(LOGL, "private struct is NULL");
	}
	a_map->private = NULL;
	p = (char *)private->ptr - private->page_ofs;
	smem_create(NULL, p, 0, SM_DETACH);
	if (0 != smem_remove(private->name)) {
		log_error(LOGL, "smem_remove(%s): %s.", private->name,
		    strerror(errno));
	}
	FREE(private->name);
	FREE(private);

	LOGF(verbose)(LOGL, "sicy_unmap }");
}

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	struct SiCy *private;
	uint8_t *p8;

	private = a_map->private;
	p8 = private->ptr;
	*(uint16_t *)(p8 + a_ofs) = a_u16;
}

void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	struct SiCy *private;
	uint8_t *p8;

	private = a_map->private;
	p8 = private->ptr;
	*(uint32_t *)(p8 + a_ofs) = a_u32;
}

#endif
