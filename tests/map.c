/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019-2024
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

#include <ntest/ntest.h>
#include <module/map/internal.h>
#include <nurdlib/base.h>

NTEST(UserRegion)
{
	uint8_t user1[0x1003];
	uint8_t user2[0x1004];
	struct Map *map;

	ZERO(user1);
	ZERO(user2);

	/* The order should not matter, user1 is too small. */
	map_user_add(0x01000000, user1, sizeof user1);
	map_user_add(0x01000000, user2, sizeof user2);
	map = map_map(0x01000004, 0x1000, KW_NOBLT, 0, 0,
	    0, 0, 0,
	    0, 0, 0, 0);
	map_sicy_write(map, 4, 32, 0, 0x01010101);
	NTRY_U(user1[4], ==, 0);
	NTRY_U(user2[4], ==, 1);
	map_sicy_write(map, 4, 32, 0, 0x02020202);
	NTRY_U(user1[4], ==, 0);
	NTRY_U(user2[4], ==, 2);
	map_unmap(&map);
	map_user_clear();

	map_user_add(0x01000000, user2, sizeof user2);
	map_user_add(0x01000000, user1, sizeof user1);
	map = map_map(0x01000004, 0x1000, KW_NOBLT, 0, 0,
	    0, 0, 0,
	    0, 0, 0, 0);
	map_sicy_write(map, 4, 32, 0, 0x03030303);
	NTRY_U(user1[4], ==, 0);
	NTRY_U(user2[4], ==, 3);
	map_sicy_write(map, 4, 32, 0, 0x04040404);
	NTRY_U(user1[4], ==, 0);
	NTRY_U(user2[4], ==, 4);
	map_unmap(&map);
	map_user_clear();
}

#ifdef POKE_DUMB
static struct {
	uintptr_t	address;
	uintptr_t	ofs;
	unsigned	bits;
	uint32_t	value;
} g_poke_r, g_poke_w;
static void
poke_callback(char const *a_mod, uintptr_t a_address, uintptr_t a_ofs,
    unsigned a_bits, uint32_t a_value)
{
	if ('r' == *a_mod) {
		g_poke_r.address = a_address;
		g_poke_r.ofs = a_ofs;
		g_poke_r.bits = a_bits;
	} else {
		g_poke_w.address = a_address;
		g_poke_w.ofs = a_ofs;
		g_poke_w.bits = a_bits;
		g_poke_w.value = a_value;
	}
}
#endif

NTEST(Poke)
{
	struct Map *map;

	(void)map;

	NTRY_SIGNAL(map =
	    map_map(0x1, sizeof(uint32_t), KW_NOBLT, 0, 0,
	    MAP_MOD_r, 2, 16,
	    0, 0, 0, 1)
	    );
	NTRY_SIGNAL(map =
	    map_map(0x2, sizeof(uint32_t), KW_NOBLT, 0, 0,
	    MAP_MOD_W, 2, 16,
	    0, 0, 0, 2)
	    );
	NTRY_SIGNAL(map =
	    map_map(0x3, sizeof(uint32_t), KW_NOBLT, 0, 0,
	    0, 2, 16,
	    0, 0, 0, 3)
	    );

	NTRY_SIGNAL(map =
	    map_map(0x4, sizeof(uint32_t), KW_NOBLT, 0, 0,
	    0, 0, 0,
	    MAP_MOD_R, 4, 32, 4)
	    );
	NTRY_SIGNAL(map =
	    map_map(0x5, sizeof(uint32_t), KW_NOBLT, 0, 0,
	    0, 0, 0,
	    MAP_MOD_r, 4, 32, 5)
	    );
	NTRY_SIGNAL(map =
	    map_map(0x6, sizeof(uint32_t), KW_NOBLT, 0, 0,
	    0, 0, 0,
	    0, 4, 32, 6)
	    );

#ifdef POKE_DUMB
	map_dumb_poke_callback_set(poke_callback);

	map =
	    map_map(0x7, sizeof(uint32_t), KW_NOBLT, 0, 0,
		MAP_MOD_R, 2, 16,
		0, 0, 0, 7);
	map_unmap(&map);
	NTRY_U(0x7, ==, g_poke_r.address);
	NTRY_U(  2, ==, g_poke_r.ofs);
	NTRY_U( 16, ==, g_poke_r.bits);

	map =
	    map_map(0x8, sizeof(uint32_t), KW_NOBLT, 0, 0,
		0, 0, 0,
		MAP_MOD_W, 4, 32, 8);
	map_unmap(&map);
	NTRY_U(0x8, ==, g_poke_w.address);
	NTRY_U(  4, ==, g_poke_w.ofs);
	NTRY_U( 32, ==, g_poke_w.bits);
	NTRY_U(  8, ==, g_poke_w.value);
#endif
}

NTEST_SUITE(Map)
{
	NTEST_ADD(UserRegion);
	NTEST_ADD(Poke);
}
