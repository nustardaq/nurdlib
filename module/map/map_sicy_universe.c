/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
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

#ifdef SICY_UNIVERSE

#	include <vme/vme.h>
#	include <vme/vme_api.h>
#	include <nurdlib/log.h>

struct SiCy {
	volatile	void *ptr;
	vme_bus_handle_t	bus_handle;
	vme_master_handle_t	window_handle_A32;
};

MAP_FUNC_EMPTY(sicy_deinit);

void
sicy_map(struct Map *a_map)
{
	vme_bus_handle_t bus_handle;
	vme_master_handle_t window_handle_A32;
	struct SiCy *private;

	LOGF(verbose)(LOGL, "sicy_map {");

	CALLOC(private, 1);

	if (vme_init(&bus_handle)) {
		log_die(LOGL, "UNIVERSE vme_init failed.");
	}
	if (0 != vme_master_window_create(bus_handle, &window_handle_A32,
	    (uint64_t)a_map->address, VME_A32UD, a_map->bytes,
	    VME_CTL_MAX_DW_32, NULL)) {
		log_die(LOGL,"Error creating master A32 window "
		    "(addr=0x%08x, size=0x%08"PRIzx").", a_map->address,
		    a_map->bytes);
	}
	private->ptr = (volatile void *)vme_master_window_map(bus_handle,
	    window_handle_A32, 0);
	if (NULL == private->ptr) {
		log_die(LOGL,"Error mapping master A32 window "
		    "(addr=0x%08x, size=0x%08"PRIzx")", a_map->address,
		    a_map->bytes);
	}
	private->bus_handle = bus_handle;
	private->window_handle_A32 = window_handle_A32;

	a_map->private = private;

	LOGF(verbose)(LOGL, "sicy_map(bus=%p, window=%p, ptr=%p) }",
	    (void *)(uintptr_t)bus_handle,
	    (void *)(uintptr_t)window_handle_A32, private->ptr);
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	struct SiCy *private;
	uint8_t volatile const *p8;

	private = a_map->private;
	p8 = private->ptr;
	return *(uint16_t volatile const *)(p8 + a_ofs);
}

uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	struct SiCy *private;
	uint8_t volatile const *p8;

	private = a_map->private;
	p8 = private->ptr;
	return *(uint16_t volatile const *)(p8 + a_ofs);
}

MAP_FUNC_EMPTY(sicy_setup);
MAP_FUNC_EMPTY(sicy_shutdown);

void
sicy_unmap(struct Map *a_map)
{
	struct SiCy *private;

	LOGF(verbose)(LOGL, "sicy_unmap {");

	private = a_map->private;
	if (!private) {
		log_die(LOGL, "private struct is NULL.");
	}
	a_map->private = NULL;
	vme_master_window_unmap(private->bus_handle,
	    private->window_handle_A32);
	vme_master_window_release(private->bus_handle,
	    private->window_handle_A32);
	vme_term(private->bus_handle);
	FREE(private);

	LOGF(verbose)(LOGL, "sicy_unmap }");
}

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	struct SiCy *private;
	uint8_t volatile *p8;

	private = a_map->private;
	p8 = private->ptr;
	*(uint16_t volatile *)(p8 + a_ofs) = a_u16;
}

void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	struct SiCy *private;
	uint8_t volatile *p8;

	private = a_map->private;
	p8 = private->ptr;
	*(uint32_t volatile *)(p8 + a_ofs) = a_u32;
}
#endif

#ifdef POKE_UNIVERSE

#	define POKE_BEGIN(type_) {\
		struct SiCy *private;\
		struct Map mapper;\
		void volatile *p;\
		mapper.type = MAP_TYPE_SICY;\
		mapper.address = a_address + a_ofs_##type_;\
		mapper.bytes = 4;\
		sicy_map(&mapper);\
		private = mapper.private;\
		p = private->ptr;\
		LOGF(verbose)(LOGL, "Poke_"#type_" %08x+%"PRIpx" bits=%u...",\
		    a_address, a_ofs_##type_, a_bits_##type_);\
		switch (a_bits_##type_) {
#	define POKE_END }\
		sicy_unmap(&mapper);\
	}

void
poke_r(uint32_t a_address, uintptr_t a_ofs_r, unsigned a_bits_r)
{
	uint32_t value;
	int bad;

	POKE_BEGIN(r)
	case 16:
		value = *(uint16_t volatile *)p;
		bad = value == 0xFFFF;
		break;
	case 32:
		value = *(uint32_t volatile *)p;
		bad = value == 0xFFFFFFFF;
		break;
	default:
		log_die(LOGL, "Internal error.");
	POKE_END;
	if (bad) {
		log_die(LOGL,
		    "Poke reading 0x%08x+0x%"PRIpx" failed, wrong address?",
		    a_address, a_ofs_r);
	}
}

void
poke_w(uint32_t a_address, uintptr_t a_ofs_w, unsigned a_bits_w, uint32_t
    a_value)
{
	POKE_BEGIN(w)
	case 16: *(uint16_t volatile *)p = a_value; break;
	case 32: *(uint32_t volatile *)p = a_value; break;
	default: log_die(LOGL, "Internal error.");
	POKE_END
}

#endif
