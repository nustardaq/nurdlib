/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2025
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
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/assert.h>
#include <util/fmtmod.h>
#include <util/queue.h>
#include <util/string.h>
#include <util/time.h>

TAILQ_HEAD(UserList, User);
struct User {
	uint32_t	address;
	uint8_t	*ptr;
	size_t	bytes;
	TAILQ_ENTRY(User)	next;
};

static int	blt_read_common(struct Map *, size_t, void *, size_t, int)
	FUNC_RETURNS;

static struct UserList g_user_list = TAILQ_HEAD_INITIALIZER(g_user_list);

#ifndef BLT_HW_MBLT_SWAP
static uint32_t	mblt_swap(uint32_t*, size_t) FUNC_RETURNS;
#endif

int
blt_read_common(struct Map *a_mapper, size_t a_offset, void *a_target, size_t
    a_bytes, int a_berr_ok)
{
	int ret;

	LOGF(spam)(LOGL, "blt_read_common(source=0x%08x,offset=0x%"PRIzx","
	    "target=%p,bytes=0x%"PRIzx",berr=%s) {",
	    a_mapper->address, a_offset, a_target, a_bytes,
	    a_berr_ok ? "yes" : "no");
	ret = blt_read(a_mapper, a_offset, a_target, a_bytes, a_berr_ok);
#ifndef BLT_HW_MBLT_SWAP
	/* TODO: Go through these macro and soft switches. */
	if (0 == ret &&
	    KW_MBLT == a_mapper->mode &&
	    a_mapper->do_mblt_swap) {
		ret = mblt_swap(a_target, a_bytes);
	}
#endif
	LOGF(spam)(LOGL, "blt_read_common(bytes=%d) }", ret);
	return ret;
}

void *
map_align(void *a_ptr, size_t *a_bytes, enum Keyword a_blt_mode, uint32_t
    a_filler)
{
	uint32_t *p32;
	unsigned mask;

	LOGF(spam)(LOGL, "map_align(ptr=%p,bytes=0x%"PRIzx",blt_mode=%s,"
	    "filler=0x%08x) {",
	    a_ptr, *a_bytes, keyword_get_string(a_blt_mode), a_filler);
	if (0 != ((uintptr_t)a_ptr & 3) ||
	    0 != (*a_bytes & 3)) {
		log_die(LOGL, "map_align expects 32-bit aligned pointers "
		    "and bytes!");
	}
	if (KW_BLT == a_blt_mode || KW_FF == a_blt_mode) {
		mask = sizeof(uint32_t) - 1;
	} else if (KW_MBLT == a_blt_mode) {
		mask = 2 * sizeof(uint32_t) - 1;
	} else if (KW_BLT_2ESST == a_blt_mode ||
	    KW_BLT_2EVME == a_blt_mode) {
		mask = 4 * sizeof(uint32_t) - 1;
	} else {
		log_die(LOGL, "map_align does not support blt_mode=%s!",
		    keyword_get_string(a_blt_mode));
	}
	for (p32 = a_ptr; 0 != ((uintptr_t)p32 & mask); ++p32) {
		*p32 = a_filler;
	}
	*a_bytes = (*a_bytes + mask) & ~mask;
	LOGF(spam)(LOGL, "map_align(ptr=%p,bytes=0x%"PRIzx") }",
	    (void *)p32, *a_bytes);
	return p32;
}

int
map_blt_read(struct Map *a_mapper, size_t a_offset, void *a_target, size_t
    a_bytes)
{
	return blt_read_common(a_mapper, a_offset, a_target, a_bytes, 0);
}

int
map_blt_read_berr(struct Map *a_mapper, size_t a_offset, void *a_target,
    size_t a_bytes)
{
	return blt_read_common(a_mapper, a_offset, a_target, a_bytes, 1);
}

void
map_deinit(void)
{
	LOGF(verbose)(LOGL, "map_deinit {");
	blt_deinit();
	sicy_deinit();
	LOGF(verbose)(LOGL, "map_deinit }");
}

struct Map *
map_map(uint32_t a_address, size_t a_bytes, enum Keyword a_blt_mode, int
    a_do_fifo, int a_do_mblt_swap,
    unsigned a_poke_r_mod, uintptr_t a_poke_r_ofs, unsigned a_poke_r_bits,
    unsigned a_poke_w_mod, uintptr_t a_poke_w_ofs, unsigned a_poke_w_bits,
    uint32_t a_poke_w_value)
{
	struct Map *mapper;

	LOGF(verbose)(LOGL, "map_map(addr=0x%08x,bytes=0x%08"PRIzx",mode=%s,"
	    "fifo=%s,poke_r=0x%"PRIpx"/%u,poke_w=0x%08"PRIpx"/%u/0x%08x) {",
	    a_address, a_bytes, keyword_get_string(a_blt_mode), a_do_fifo ?
	    "yes" : "no",
	    a_poke_r_ofs, a_poke_r_bits,
	    a_poke_w_ofs, a_poke_w_bits, a_poke_w_value);

	if (0 != a_poke_r_bits && 0 == (MAP_MOD_R & a_poke_r_mod)) {
		log_die(LOGL, "Poke-read with non-readable register!");
	}
	if (0 != a_poke_w_bits && 0 == (MAP_MOD_W & a_poke_w_mod)) {
		log_die(LOGL, "Poke-read with non-writable register!");
	}

	/* User regions don't need poking, test first. */
	if (KW_NOBLT == a_blt_mode) {
		struct User const *user;

		TAILQ_FOREACH(user, &g_user_list, next) {
			if (user->address <= a_address &&
			    user->address + user->bytes >= a_address +
			    a_bytes) {
				CALLOC(mapper, 1);
				mapper->type = MAP_TYPE_USER;
				mapper->address = a_address;
				mapper->bytes = a_bytes;
				mapper->private = user->ptr + a_address -
				    user->address;
				LOGF(verbose)(LOGL, "User mapping chosen "
				    "(addr=0x%08x, ptr=%p, bytes=%"PRIz").",
				    user->address,
				    (void *)user->ptr,
				    user->bytes);
				goto map_map_done;
			}
		}
	}

	/* Poke region before mapping with "dangerous" methods. */
	if (0 != a_poke_r_bits) {
		LOGF(verbose)(LOGL, "Poking read access {");
		poke_r(a_address, a_poke_r_ofs, a_poke_r_bits);
		LOGF(verbose)(LOGL, "Poking read access }");
	}
	if (0 != a_poke_w_bits) {
		LOGF(verbose)(LOGL, "Poking write access {");
		poke_w(a_address, a_poke_w_ofs, a_poke_w_bits,
		    a_poke_w_value);
		LOGF(verbose)(LOGL, "Poking write access }");
	}

	CALLOC(mapper, 1);
	mapper->mode = a_blt_mode;
	mapper->address = a_address;
	mapper->bytes = a_bytes;
	mapper->do_mblt_swap = a_do_mblt_swap;

	if (KW_NOBLT == a_blt_mode) {
		char str_r[16], str_w[16];
		double td[3] = {1e9, 1e9, 1e9};
		unsigned sum, trial;

		mapper->type = MAP_TYPE_SICY;
		sicy_map(mapper);

		/*
		 * Measure time for reads/writes on the SiCy poke register,
		 * impl idea borrowed from drasi.
		 * Don't do this while poking, it may use slower access!
		 */
		sum = 0;
		(void)sum;
		for (trial = 0; trial < 5; ++trial) {
#define TSAMPLE(id) do {\
		double dt = t1 - t0;\
		if (dt < td[id]) {\
			td[id] = dt;\
		}\
	} while (0)
			double t0, t1;
			unsigned i;

			t0 = time_getd();
			t1 = time_getd();
			TSAMPLE(0);

			LOGF(verbose)(LOGL, "Measuring read access speed {");
			t0 = time_getd();
			switch (a_poke_r_bits) {
			case 32:
				for (i = 0; i < 1000; ++i) {
					sum += map_sicy_read(mapper,
					    MAP_MOD_R, 32, a_poke_r_ofs);
				}
				break;
			case 16:
				for (i = 0; i < 1000; ++i) {
					sum += map_sicy_read(mapper,
					    MAP_MOD_R, 16, a_poke_r_ofs);
				}
				break;
			default:
				break;
			}
			t1 = time_getd();
			TSAMPLE(1);
			LOGF(verbose)(LOGL, "Measuring read access speed }");

			LOGF(verbose)(LOGL, "Measuring write access speed {");
			t0 = time_getd();
			switch (a_poke_w_bits) {
			case 32:
				for (i = 0; i < 1000; ++i) {
					map_sicy_write(mapper, MAP_MOD_W, 32,
					    a_poke_w_ofs, a_poke_w_value);
				}
				break;
			case 16:
				for (i = 0; i < 1000; ++i) {
					map_sicy_write(mapper, MAP_MOD_W, 16,
					    a_poke_w_ofs, a_poke_w_value);
				}
				break;
			default:
				break;
			}
			t1 = time_getd();
			TSAMPLE(2);
			LOGF(verbose)(LOGL, "Measuring write access speed }");
		}

		if (0 == a_poke_r_bits) {
			strlcpy_(str_r, "skipped", sizeof str_r);
		} else {
			snprintf_(str_r, sizeof str_r, "%dns",
			    (int)(1e6 * (td[1] - td[0])));
		}
		if (0 == a_poke_w_bits) {
			strlcpy_(str_w, "skipped", sizeof str_w);
		} else {
			snprintf_(str_w, sizeof str_w, "%dns",
			    (int)(1e6 * (td[2] - td[0])));
		}
		if (0 < a_poke_r_bits &&
		    0 < a_poke_w_bits) {
			LOGF(info)(LOGL, "rd(0x%08x+0x%"PRIzx"/%u)=%s "
			    "wr(0x%08x+0x%"PRIzx"/%u)=%s.",
			    a_address, a_poke_r_ofs, a_poke_r_bits, str_r,
			    a_address, a_poke_w_ofs, a_poke_w_bits, str_w);
		}
	} else {
		mapper->type = MAP_TYPE_BLT;
		blt_map(mapper, a_blt_mode, a_do_fifo, a_do_mblt_swap);
	}
map_map_done:
	LOGF(verbose)(LOGL, "map_map }");
	return mapper;
}

void
map_setup(void)
{
	LOGF(verbose)(LOGL, "map_setup {");
	sicy_setup();
	blt_setup();
	LOGF(verbose)(LOGL, "Broken BLT return val = "
#ifdef MAP_BLT_RETURN_BROKEN
	    "yes"
#else
	    "no"
#endif
	".");
	LOGF(verbose)(LOGL, "map_setup }");
}

void
map_shutdown(void)
{
	LOGF(verbose)(LOGL, "map_shutdown {");
	map_deinit();
	blt_shutdown();
	sicy_shutdown();
	LOGF(verbose)(LOGL, "map_shutdown }");
}

void
map_unmap(struct Map **a_mapper)
{
	struct Map *mapper;

	LOGF(verbose)(LOGL, "map_unmap {");
	mapper = *a_mapper;
	if (NULL != mapper) {
		LOGF(verbose)(LOGL, "Unmap 0x%08x:0x%"PRIzx".",
		    mapper->address, mapper->bytes);
		switch (mapper->type) {
		case MAP_TYPE_SICY:
			sicy_unmap(mapper);
			break;
		case MAP_TYPE_BLT:
			blt_unmap(mapper);
			break;
		case MAP_TYPE_USER:
			break;
		}
		FREE(*a_mapper);
	}
	LOGF(verbose)(LOGL, "map_unmap }");
}

void
map_user_add(uint32_t a_address, void *a_user, size_t a_user_bytes)
{
	struct User *user;

	CALLOC(user, 1);
	user->address = a_address;
	user->ptr = a_user;
	user->bytes = a_user_bytes;
	TAILQ_INSERT_TAIL(&g_user_list, user, next);
}

void
map_user_clear(void)
{
	while (!TAILQ_EMPTY(&g_user_list)) {
		struct User *user;

		user = TAILQ_FIRST(&g_user_list);
		TAILQ_REMOVE(&g_user_list, user, next);
		FREE(user);
	}
}


#ifndef BLT_HW_MBLT_SWAP
uint32_t
mblt_swap(uint32_t *a_p32, size_t a_bytes)
{
	size_t i;

	if (0 != (7 & a_bytes)) {
		log_error(LOGL, "MBLT swap data size not 64-bit aligned!");
		return CRATE_READOUT_FAIL_ERROR_DRIVER;
	}
	for (i = 0; i < a_bytes; i += 8) {
		uint32_t swap;

		swap = a_p32[0];
		a_p32[0] = a_p32[1];
		a_p32[1] = swap;
		a_p32 += 2;
	}
	return 0;
}
#endif

uint32_t
map_sicy_read(struct Map *a_map, unsigned a_mod, unsigned a_bits, size_t
    a_ofs)
{
	ASSERT(unsigned, "u", 0, !=, a_mod & (MAP_MOD_R | MAP_MOD_r));
	if (MAP_TYPE_USER == a_map->type) {
		uint8_t const *p8 = a_map->private;

		switch (a_bits) {
		case 16: return *(uint16_t const *)(p8 + a_ofs);
		case 32: return *(uint32_t const *)(p8 + a_ofs);
		default: abort();
		}
	}
	switch (a_bits) {
	case 16: return sicy_r16(a_map, a_ofs);
	case 32: return sicy_r32(a_map, a_ofs);
	default: abort();
	}
}

void
map_sicy_write(struct Map *a_map, unsigned a_mod, unsigned a_bits, size_t
    a_ofs, uint32_t a_val)
{
	ASSERT(unsigned, "u", 0, !=, a_mod & MAP_MOD_W);
	if (MAP_TYPE_USER == a_map->type) {
		uint8_t *p8 = a_map->private;

		switch (a_bits) {
		case 16: *(uint16_t *)(p8 + a_ofs) = a_val; break;
		case 32: *(uint32_t *)(p8 + a_ofs) = a_val; break;
		default: abort();
		}
	} else {
		switch (a_bits) {
		case 16: sicy_w16(a_map, a_ofs, a_val); break;
		case 32: sicy_w32(a_map, a_ofs, a_val); break;
		default: abort();
		}
	}
}
