/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
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
#include <util/sigbus.h>

#ifdef BLT_BMA

#	include <sys/types.h>
#	include <ces/bmalib.h>
#	include <nurdlib/log.h>

struct BLT {
	void	*ptr;
	enum	Keyword mode;
	int	do_fifo;
};

static void	dma_configure(struct BLT const *);
static void	mode_to_bma_values(enum Keyword, int, int *, int *, int *);

MAP_FUNC_EMPTY(blt_deinit);

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int
    a_do_mblt_swap)
{
	struct BLT *private;

	(void) a_do_mblt_swap;
	
	LOGF(verbose)(LOGL, "blt_map {");
	CALLOC(private, 1);
	private->ptr = (void *)(uintptr_t)(BLT_BMA_D32_BASE + a_map->address);
	private->mode = a_mode;
	private->do_fifo = a_do_fifo;
	a_map->private = private;
	dma_configure(private);
	LOGF(verbose)(LOGL, "blt_map }");
}

int
blt_read(struct Map *a_map, size_t a_offset, void *a_target, size_t a_bytes,
    int a_berr_ok)
{
	struct BLT *private;
	int bytes, ret, word_count;

	private = a_map->private;
	LOGF(spam)(LOGL, "blt_read(source=%p, offset=0x%08x, target=%p, "
	    "bytes=%d) {",
	    private->ptr,
	    (uint32_t)a_offset,
	    a_target,
	    (int)a_bytes);

#define CHECK_ALIGN_32(Type, var)\
	do {\
		if (0 != (3 & (Type)var)) {\
			log_die(LOGL, #var" (%u) not on 32-bit boundary.",\
			    (uint32_t)(Type)var);\
		}\
	} while (0)
	CHECK_ALIGN_32(uint32_t, a_offset);
	CHECK_ALIGN_32(uintptr_t, a_target);
	CHECK_ALIGN_32(uint32_t, a_bytes);

	dma_configure(private);
	word_count = a_bytes / sizeof(uint32_t);
	if (a_berr_ok) {
		sigbus_set_ok(1);
	}
	ret = bma_read_count((uintptr_t)private->ptr + a_offset,
	    0x0fffffff & (uintptr_t)a_target, word_count, BMA_DEFAULT_MODE);
	if (a_berr_ok) {
		sigbus_set_ok(0);
	}
	if (0 > ret) {
		bma_perror("bma_read_count", ret);
		log_error(LOGL, "bma_read_count(word_count=%d)=%d failed.",
		    word_count, ret);
		bytes = -1;
	} else {
		bytes = ret * sizeof(uint32_t);
	}

	LOGF(spam)(LOGL, "blt_read(bytes=%d) }", bytes);
	return bytes;
}

MAP_FUNC_EMPTY(blt_setup);
MAP_FUNC_EMPTY(blt_shutdown);

void
blt_unmap(struct Map *a_map)
{
	FREE(a_map->private);
}

void
dma_configure(struct BLT const *a_blt)
{
	static int s_is_bma_open = 0;
	static int s_addr_inc = -1, s_addr_mod = -1, s_vme_size = -1,
		   s_word_size = -1;
	int ret;
	int addr_mod, word_size, addr_inc;

	LOGF(spam)(LOGL, "dma_configure(mode=%s,fifo=%d) {",
	    keyword_get_string(a_blt->mode), a_blt->do_fifo);

	if (KW_NOBLT == a_blt->mode) {
		log_die(LOGL, "Should not configure DMA for no BLT.");
	}

	if (!s_is_bma_open) {
		ret = bma_open();
		if (0 != ret) {
			bma_perror("bma_open", ret);
			log_die(LOGL, "bma_open failed with code %d.", ret);
		}
		s_is_bma_open = 1;
	}
#define BMA_SET_MODE(param, value, prev)\
	do {\
		if (value == prev) {\
			break;\
		}\
		prev = value;\
		ret = bma_set_mode(BMA_DEFAULT_MODE, param, value);\
		if (0 != ret) {\
			bma_perror("bma_set_mode("#param", "#value")", ret);\
			bma_close();\
			log_die(LOGL, "bma_set_mode("#param", %d) failed "\
			    "with code %d.", value, ret);\
		}\
	} while (0)
	mode_to_bma_values(a_blt->mode, a_blt->do_fifo, &addr_inc, &addr_mod,
	    &word_size);
	BMA_SET_MODE(BMA_M_AmCode, addr_mod, s_addr_mod);
	BMA_SET_MODE(BMA_M_VMESize, 6, s_vme_size);
	BMA_SET_MODE(BMA_M_WordSize, word_size, s_word_size);
	BMA_SET_MODE(BMA_M_VMEAdrInc, addr_inc, s_addr_inc);

	LOGF(spam)(LOGL, "dma_configure }");
}

/* map_blt_dst_* handled by blt_dumb */

void
mode_to_bma_values(enum Keyword a_mode, int a_do_fifo, int *a_addr_inc, int
    *a_addr_mod, int *a_word_size)
{
	LOGF(spam)(LOGL, "mode_to_bma_values(mode=%s,fifo=%s) {",
	    keyword_get_string(a_mode), a_do_fifo ? "yes" : "no");

	switch (a_mode) {
	case KW_BLT:
		*a_addr_mod = BMA_M_AmA32U;
		*a_word_size = BMA_M_WzD32;
		break;
	case KW_MBLT:
	case KW_BLT_2ESST:
		*a_addr_mod = BMA_M_MbltU;
		*a_word_size = BMA_M_WzD64;
		break;
	default:
		abort();
	}
	if (a_do_fifo) {
		*a_addr_inc = BMA_M_VaiFifo;
	} else {
		*a_addr_inc = BMA_M_VaiNormal;
	}

	LOGF(spam)(LOGL, "mode_to_bma_values(inc=%d,mod=%d,size=%d) }",
	    *a_addr_inc, *a_addr_mod, *a_word_size);
}

#endif
