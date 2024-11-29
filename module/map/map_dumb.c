/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
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
#include <util/assert.h>

#ifdef SICY_DUMB

#	include <nurdlib/log.h>

MAP_FUNC_EMPTY(sicy_deinit);

void
sicy_map(struct Map *a_map)
{
	(void)a_map;
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	(void)a_map;
	(void)a_ofs;
	return 0;
}

uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	(void)a_map;
	(void)a_ofs;
	return 0;
}

MAP_FUNC_EMPTY(sicy_setup);
MAP_FUNC_EMPTY(sicy_shutdown);
UNMAP_FUNC_EMPTY(sicy);

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	(void)a_map;
	(void)a_ofs;
	(void)a_u16;
}

void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	(void)a_map;
	(void)a_ofs;
	(void)a_u32;
}

#endif

#ifdef POKE_DUMB

static MapDumbPokeCallback g_callback;

void
map_dumb_poke_callback_set(MapDumbPokeCallback a_callback)
{
	g_callback = a_callback;
}

void
poke_r(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits)
{
	if (g_callback) {
		g_callback("r", a_address, a_ofs, a_bits, 0);
	}
}

void
poke_w(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits, uint32_t a_value)
{
	if (g_callback) {
		g_callback("w", a_address, a_ofs, a_bits, a_value);
	}
}

#endif

#ifdef BLT_DUMB

MAP_FUNC_EMPTY(blt_deinit);

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int
    a_do_mblt_swap)
{
	(void)a_map;
	(void)a_mode;
	(void)a_do_fifo;
	(void)a_do_mblt_swap;
	log_die(LOGL, "Platform does not support dynamic mapping.");
}

int
blt_read(struct Map *a_mapper, size_t a_offset, void *a_target, size_t
    a_bytes, int a_berr_ok)
{
	(void)a_mapper;
	(void)a_offset;
	(void)a_target;
	(void)a_bytes;
	(void)a_berr_ok;
	log_die(LOGL, "Platform does not support DMA reads.");
}

MAP_FUNC_EMPTY(blt_setup);
MAP_FUNC_EMPTY(blt_shutdown);
UNMAP_FUNC_EMPTY(blt);

#endif

#ifdef BLT_DST_DUMB

struct MapBltDst {
	void	*ptr;
};

struct MapBltDst *
map_blt_dst_alloc(size_t a_bytes)
{
	struct MapBltDst *dst;

	CALLOC(dst, 1);
	dst->ptr = malloc(a_bytes);
	if (NULL == dst->ptr) {
		log_err(LOGL, "malloc(%"PRIz")", a_bytes);
	}
	return dst;
}

void
map_blt_dst_free(struct MapBltDst **a_dst)
{
	struct MapBltDst *dst;

	dst = *a_dst;
	if (dst) {
		FREE(dst->ptr);
		FREE(*a_dst);
	}
}

void *
map_blt_dst_get(struct MapBltDst *a_dst)
{
	return a_dst->ptr;
}

#endif
