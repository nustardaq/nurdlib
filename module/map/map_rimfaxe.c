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
#include <nurdlib/log.h>

#if defined(SICY_RIMFAXE) || defined(BLT_RIMFAXE_FF)

#	include <rimfaxe/avb.h>
#	include <nurdlib/log.h>

static void	rimfaxe_avb_setup(void);
static void	rimfaxe_avb_shutdown(void);

static struct avb *g_avb;

void
rimfaxe_avb_setup(void)
{
	int ret;

	if (NULL != g_avb) {
		return;
	}
	LOGF(verbose)(LOGL, "rimfaxe_avb_setup {");
	ret = avb_init(&g_avb);
	if (ret) {
		log_die(LOGL, "avb_init failed with error code %d.", ret);
	}
	LOGF(verbose)(LOGL, "rimfaxe_avb_setup }");
}

void
rimfaxe_avb_shutdown(void)
{
	if (NULL == g_avb) {
		return;
	}
	LOGF(verbose)(LOGL, "rimfaxe_avb_shutdown {");
	avb_free(g_avb);
	g_avb = NULL;
	LOGF(verbose)(LOGL, "rimfaxe_avb_shutdown }");
}

#endif

#ifdef SICY_RIMFAXE

struct SiCy {
	void	volatile *ptr;
};

void
sicy_deinit(void)
{
	LOGF(verbose)(LOGL, "sicy_deinit {");
	rimfaxe_avb_shutdown();
	LOGF(verbose)(LOGL, "sicy_deinit }");
}

void
sicy_map(struct Map *a_map)
{
	struct SiCy *sicy;

	LOGF(verbose)(LOGL, "sicy_map {");
	rimfaxe_avb_setup();
	CALLOC(sicy, 1);
	sicy->ptr = avb_map(g_avb, a_map->address, a_map->bytes, AM_A32_DATA,
	    AVB_MASK_NONE);
	if (NULL == sicy->ptr) {
		log_die(LOGL, "avb_map addr=0x%08x bytes=0x%"PRIzx" failed: "
		    "%s.",
		    a_map->address, a_map->bytes, avb_get_err_msg(g_avb));
	}
	a_map->private = sicy;
	LOGF(verbose)(LOGL, "sicy_map }");
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	struct SiCy *sicy;
	uint8_t const volatile *p8;

	sicy = a_map->private;
	p8 = sicy->ptr;
	return *(uint16_t const volatile *)(p8 + a_ofs);
}

uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	struct SiCy *sicy;
	uint8_t const volatile *p8;

	sicy = a_map->private;
	p8 = sicy->ptr;
	return *(uint32_t const volatile *)(p8 + a_ofs);
}

MAP_FUNC_EMPTY(sicy_setup);
MAP_FUNC_EMPTY(sicy_shutdown);

void
sicy_unmap(struct Map *a_map)
{
	struct SiCy *sicy;

	LOGF(verbose)(LOGL, "sicy_unmap {");
	sicy = a_map->private;
	avb_unmap(g_avb, sicy->ptr);
	FREE(a_map->private);
	LOGF(verbose)(LOGL, "sicy_unmap }");
}

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	struct SiCy *sicy;
	uint8_t volatile *p8;

	sicy = a_map->private;
	p8 = sicy->ptr;
	*(uint16_t volatile *)(p8 + a_ofs) = a_u16;
}

void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	struct SiCy *sicy;
	uint8_t volatile *p8;

	sicy = a_map->private;
	p8 = sicy->ptr;
	*(uint32_t volatile *)(p8 + a_ofs) = a_u32;
}

#	ifdef POKE_RIMFAXE

void
poke_r(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits)
{
	avb_err_code err = 0;
	uint32_t target = a_address + a_ofs;

	switch (a_bits) {
	case 16: {
		uint16_t v;
		err = avb_safe_read16(g_avb, target, &v);
		}
		break;
	case 32: {
		uint32_t v;
		err = avb_safe_read32(g_avb, target, &v);
		}
		break;
	default:
		log_die(LOGL, "Rimfaxe poke does not support %u bits.",
		    a_bits);
	}
	if (0 != err) {
		log_die(LOGL, "Poking read addr=0x%08x+0x%08"PRIpx" "
		    "bits=%u failed: %s.", a_address, a_ofs,
		    a_bits, avb_get_err_msg(g_avb));
	}
}

void
poke_w(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits, uint32_t a_value)
{
	(void)a_address;
	(void)a_ofs;
	(void)a_bits;
	(void)a_value;
}

#	else
#		error "SICY_RIMFAXE defined but not POKE_RIMFAXE!?"
#	endif

#endif


#ifdef BLT_RIMFAXE_FF

#	include <rimfaxe/avb-ff.h>

struct avb_ff_private {
	struct	avb_ff *normal;
	struct	avb_ff *berr;
};

void
blt_deinit(void)
{
	LOGF(verbose)(LOGL, "blt_deinit {");
	rimfaxe_avb_shutdown();
	LOGF(verbose)(LOGL, "blt_deinit }");
}

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int
    a_do_mblt_swap)
{
	struct avb_ff_private *private;
	enum AVB_AM am;
	int ret;

	(void)a_do_fifo;

	LOGF(verbose)(LOGL, "blt_map {");

	rimfaxe_avb_setup();

	if (KW_FF == a_mode) {
		am = AM_A32_DATA;
	} else if (KW_BLT == a_mode) {
		am = AM_A32_BLT;
	} else if (KW_MBLT == a_mode) {
		am = AM_A32_MBLT;
	} else {
		log_die(LOGL, "Unknown mapping mode %d=%s.",
		    a_mode, keyword_get_string(a_mode));
	}

	CALLOC(private, 1);

	private->normal = avb_ff_map(g_avb, a_map->address, a_map->bytes, am,
	    AVB_FF_NOMASK);
	if (NULL == private->normal) {
		log_die(LOGL, "avb_ff_map failed: %s.",
		    avb_get_err_msg(g_avb));
	}

	ret = avb_ff_set_mblt_swap(private->normal, a_do_mblt_swap);
	if (ret) {
		log_die(LOGL, "avb_ff_set_mblt_swap failed: %s.",
		    avb_get_err_msg(g_avb));
	}

	private->berr = avb_ff_map(g_avb, a_map->address, a_map->bytes, am,
	    AVB_FF_MASK_BERR);
	if (NULL == private->berr) {
		log_die(LOGL, "avb_ff_map failed: %s.",
		    avb_get_err_msg(g_avb));
	}

	ret = avb_ff_set_mblt_swap(private->berr, a_do_mblt_swap);
	if (ret) {
		log_die(LOGL, "avb_ff_set_mblt_swap failed: %s.",
		    avb_get_err_msg(g_avb));
	}

	a_map->private = private;

	LOGF(verbose)(LOGL, "blt_map }");
}

int
blt_read(struct Map *a_mapper, size_t a_offset, void *a_target, size_t
    a_bytes, int a_berr_ok)
{
	ssize_t read;
	struct avb_ff_private *private;

	private = a_mapper->private;

	LOGF(spam)(LOGL, "blt_read(ofs=0x%" PRIzx ", target=%p, bytes=%" PRIz
	    ") {",
	    a_offset, a_target, a_bytes);
	if (a_berr_ok) {
		read = avb_ff_read_to_berr(private->berr, (uint32_t)a_offset,
		    a_bytes, a_target);
	} else {
		read = avb_ff_read(private->normal, (uint32_t)a_offset,
		    a_bytes, a_target);
	}

	LOGF(spam)(LOGL, "blt_read(read=%d) }", (int)read);

	return (int)read;
}

MAP_FUNC_EMPTY(blt_setup);
MAP_FUNC_EMPTY(blt_shutdown);

void
blt_unmap(struct Map *a_mapper)
{
	struct avb_ff_private *private;

	LOGF(verbose)(LOGL, "blt_unmap {");
	private = a_mapper->private;
	a_mapper->private = NULL;
	avb_ff_unmap(g_avb, private->normal);
	avb_ff_unmap(g_avb, private->berr);
	FREE(private);
	LOGF(verbose)(LOGL, "blt_unmap }");
}

#endif
