/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2021, 2023-2024
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

#ifndef MODULE_GSI_PEX_INTERNAL_H
#define MODULE_GSI_PEX_INTERNAL_H

#include <module/gsi_pex/nconf.h>
#include <module/module.h>
#include <nurdlib/log.h>
#include <assert.h>
#include <stddef.h>

#define REG_DATA_RED   0xffffb0
#define REG_MEM_DIS    0xffffb4
#define REG_BUF0       0xffffd0
#define REG_BUF1       0xffffd4
#define REG_SUBMEM_NUM 0xffffd8
#define REG_SUBMEM_OFF 0xffffdc
#define REG_HEADER     0xffffe4
#define REG_DATA_LEN   0xffffec
#define REG_RESET      0xfffff4

#define GSI_PEX_READ(pex, reg) \
	(32 == BITS_##reg \
	 ? *(uint32_t volatile *)( \
	     (uint8_t volatile *)(pex)->bar0 + OFS_##reg) \
	 : (uint32_t)-1)
#define GSI_PEX_WRITE(pex, reg, val) \
	if (32 == BITS_##reg) { \
		*(uint32_t volatile *)( \
		    (uint8_t volatile *)(pex)->bar0 + OFS_##reg) = (val); \
	} else { \
		abort(); \
	}

enum GsiPexGocCmd {
	GSI_PEX_GOC_READ = 0,
	GSI_PEX_GOC_WRITE = 1
};

struct GsiPexDma {
	uint32_t	src;
	uint32_t	dst;
	uint32_t	transport_size;
	uint32_t	burst_size;
	uint32_t	stat; /* aka dma_ctl */
	/* new pex5 registers */
	uint32_t        irq_ctl;   /* unused */
	uint32_t        irq_stat;  /* unused */
	uint32_t        dst_high; 
};

/*static_assert(offsetof(GsiPexDma, dst_high)==0x1c); */


struct GsiPexGoc {
	enum	GsiPexGocCmd cmd;
	uint8_t	sfp_i;
	uint8_t	slave_i;
	uint32_t	slave_ofs;
	union {
		uint32_t	read_id;
		uint32_t	write_value;
	} data;
};
struct GsiPexMem {
	uintptr_t	phys;
	void	volatile *virt;
};
struct GsiPex {
	int	is_v5;
	void	*bar0;
	struct	GsiPexDma volatile *dma;
	uintptr_t	physical_minus_virtual;
	void	*buf;
	unsigned	buf_bytes;
	uintptr_t	buf_physical_minus_virtual;
	struct	GsiPexMem sfp[4];
	unsigned	sfp_bitmask;
	unsigned	buf_idx;
	int	is_parallel;
	unsigned	token_mode;
};

struct GsiPex	*gsi_pex_create(struct ConfigBlock *) FUNC_RETURNS;
void		gsi_pex_deinit(struct GsiPex *);
void		gsi_pex_free(struct GsiPex **);
/*
 * Pex buf mem usage is rather terse, so here's a little explanation:
 *  1) The DAQ backend can allocate the physical memory, in which case the user
 *     code must set the physical-minus-virtual offset via gsi_pex.h.
 *  2) The nurdlib pex code can be configured to allocate the physical memory
 *     and provide the physical-minus-virtual offset.
 * Pex-based readouts must place event data into the event-buffer given to
 * them, but can do so via the two options above:
 *  1) Do DMA into the given event-buffer with the user provided offset.
 *  2) Do DMA into the pex buffer with the internal offset and move the data
 *     to the given event-buffer.
 * Note that DMA alignment forces a move or padding words in both cases, the
 * extra work is not exclusive to option 2.
 * 'gsi_pex_buf_get' behaves as follows:
 *  1) No buf mem configured -> return 0, leave the given event-buffer
 *     untouched, and give the user-defined offset (die if not set).
 *  2) Buf mem configured -> return 1, populate the given event-buffer
 *     members, and give the internal offset.
 * So a compact way to use this:
 *  COPY(eb, *a_eb);
 *  gsi_pex_buf_get(pex, &eb, &ofs);
 *  bytes = do_dma(dma_target = eb.ptr + ofs);
 *  memmove(a_eb->ptr, eb.ptr, bytes);
 *  advance(a_eb, bytes);
 */
int		gsi_pex_buf_get(struct GsiPex const *, struct EventBuffer *,
    uintptr_t *);
void		gsi_pex_init(struct GsiPex *, struct ConfigBlock *);
void		gsi_pex_readout_prepare(struct GsiPex *);
void		gsi_pex_reset(struct GsiPex *);
void		gsi_pex_sfp_tag(struct GsiPex *, unsigned);
int		gsi_pex_slave_init(struct GsiPex *, size_t, size_t)
	FUNC_RETURNS;
int		gsi_pex_slave_read(struct GsiPex *, size_t, size_t, unsigned,
    uint32_t *) FUNC_RETURNS;
int		gsi_pex_slave_write(struct GsiPex *, size_t, size_t, unsigned,
    uint32_t) FUNC_RETURNS;
void		gsi_pex_token_issue_single(struct GsiPex *, unsigned);
int		gsi_pex_token_receive(struct GsiPex *, size_t, uint32_t)
	FUNC_RETURNS;

#endif
