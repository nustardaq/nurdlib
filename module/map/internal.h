/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
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

#ifndef MODULE_MAP_INTERNAL_H
#define MODULE_MAP_INTERNAL_H

#include <module/map/map.h>

#define MAP_FUNC_EMPTY(name) \
void \
name(void) \
{ \
	LOGF(verbose)(LOGL, #name "."); \
} \
void name(void)
#define UNMAP_FUNC_EMPTY(pre) \
void \
pre##_unmap(struct Map *a_map) \
{ \
	(void)a_map; \
	LOGF(verbose)(LOGL, #pre "_unmap."); \
} \
void pre##_unmap(struct Map *)

enum MapType {
	MAP_TYPE_SICY,
	MAP_TYPE_BLT,
	MAP_TYPE_USER
};
enum MapMod {
	/* 'R' = register readable and dumpable. */
	MAP_MOD_R = 1,
	/* 'r' = register only readable sometimes (e.g. event FIFOs). */
	MAP_MOD_r = 2,
	/* 'W' = register writable. */
	MAP_MOD_W = 4
};

struct Map {
	enum	MapType type;
	enum	Keyword mode;
	uint32_t	address;
	size_t	bytes;
	int	do_mblt_swap;
	void	*private;
};

/* System-specific mapping implementations. */

void		blt_deinit(void);
void		blt_map(struct Map *, enum Keyword, int, int);
int		blt_read(struct Map *, size_t , void *, size_t, int)
	FUNC_RETURNS;
void		blt_setup(void);
void		blt_shutdown(void);
void		blt_unmap(struct Map *);

void		poke_r(uint32_t, uintptr_t, unsigned);
void		poke_w(uint32_t, uintptr_t, unsigned, uint32_t);

void		sicy_deinit(void);
void		sicy_map(struct Map *);
uint16_t	sicy_r16(struct Map *, size_t);
uint32_t	sicy_r32(struct Map *, size_t);
void		sicy_setup(void);
void		sicy_shutdown(void);
void		sicy_unmap(struct Map *);
void		sicy_w16(struct Map *, size_t, uint16_t);
void		sicy_w32(struct Map *, size_t, uint32_t);

#ifdef POKE_DUMB
typedef void (*MapDumbPokeCallback)(char const *, uintptr_t, uintptr_t,
    unsigned, uint32_t);

void		map_dumb_poke_callback_set(MapDumbPokeCallback);
#endif

#endif
