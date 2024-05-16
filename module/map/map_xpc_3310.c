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
#include <assert.h>
#include <nurdlib/base.h>
#include <nurdlib/log.h>
#include <util/err.h>
#include <util/sigbus.h>

#define BLT_ERROR_MASK 0xffff
#define BLT_ERR_BERR 0x1b

/* Common declarations. */
#if defined(SICY_XPC_3_3_10) || defined(BLT_XPC_3_3_10)
#	include <ces/CesXpcBridge.h>
#	include <ces/CesXpcBridge_Vme.h>

#	include <fcntl.h>
#	include <sys/mman.h>
#	include <sys/stat.h>
#	include <sys/types.h>
#	include <unistd.h>

static void	sicy_init(void);
static void	blt_init(void);

static struct CesXpcBridge *g_xpc_bridge;
#endif

#ifdef BLT_XPC_3_3_10
#	include <ces/cesBus.h>
#	include <ces/cesDma.h>

struct BLT {
	struct	CesDmaChain *chain;
	CesDmaElOptions	chain_el_io_opts;
	CesUint64	xpc_addr;
};
struct MapBltDst {
	int	fd;
	void	*ptr;
	size_t	bytes;
};

static struct CesBus *g_xpc_bus;
static struct CesBus *g_virt_bus;
#endif

#if defined(SICY_XPC_3_3_10) || defined(BLT_XPC_3_3_10)

void
sicy_deinit(void)
{
	LOGF(verbose)(LOGL, "sicy_deinit {");
	if (NULL != g_xpc_bridge) {
		CesXpcBridge_Put(g_xpc_bridge);
		g_xpc_bridge = NULL;
	}
	LOGF(verbose)(LOGL, "sicy_deinit }");
}

void
sicy_init(void)
{
	LOGF(verbose)(LOGL, "sicy_init {");
	if (NULL == g_xpc_bridge) {
		g_xpc_bridge = CesXpcBridge_GetByName("VME Bridge");
		if (NULL == g_xpc_bridge) {
			log_die(LOGL, "CesXpcBridge_GetByName(VME Bridge) "
			    "failed.");
		}
	}
	LOGF(verbose)(LOGL, "sicy_init }");
}

void
sicy_map(struct Map *a_map)
{
	LOGF(verbose)(LOGL, "sicy_map {");
	sicy_init();
	if (NULL != a_map->private) {
		log_die(LOGL, "Private pointer not NULL before map!");
	}
	a_map->private = CesXpcBridge_MasterMapVirt64(g_xpc_bridge,
	    a_map->address, a_map->bytes, XPC_VME_A32_STD_USER);
	if (NULL == a_map->private) {
		log_die(LOGL, "CesXpcBridge_MasterMapVirt64 failed.");
	}
	LOGF(verbose)(LOGL, "sicy_map }");
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
	if (NULL != a_map->private) {
		CesXpcBridge_MasterUnMapVirt(g_xpc_bridge,
		    (void *)a_map->private, a_map->bytes);
		a_map->private = NULL;
	}
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

void
blt_deinit(void)
{
	LOGF(verbose)(LOGL, "blt_deinit {");
	if (NULL != g_virt_bus) {
		cesReleaseBus(g_virt_bus);
		g_virt_bus = NULL;
	}
	if (NULL != g_xpc_bus) {
		cesReleaseBus(g_xpc_bus);
		g_xpc_bus = NULL;
	}
	LOGF(verbose)(LOGL, "blt_deinit }");
}

void
blt_init(void)
{
	LOGF(verbose)(LOGL, "blt_init {");
	if (NULL == g_xpc_bridge) {
		log_die(LOGL, "XPC bridge not setup before blt_init!");
	}
	g_xpc_bus = cesGetBus(CES_BUS_XPC, 0, 0);
	if (NULL == g_xpc_bus) {
		log_die(LOGL, "Could not get XPC bus.");
	}
	g_virt_bus = cesGetBus(CES_BUS_VIRT, 0, 0);
	if (NULL == g_virt_bus) {
		log_die(LOGL, "Could not get VIRT bus.");
	}
	LOGF(verbose)(LOGL, "blt_init }");
}

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int
    a_do_mblt_swap)
{
	CesUint32 options;
	CesDmaElOptions	io_opts;
	struct BLT *private;

	(void) a_do_mblt_swap;

	LOGF(verbose)(LOGL, "blt_map {");
	CALLOC(private, 1);
	if (KW_BLT == a_mode) {
		options = XPC_VME_A32_BLT_USER;
	} else if (KW_MBLT == a_mode) {
		options = XPC_VME_A32_MBLT_USER;
	} else if (KW_BLT_2EVME == a_mode) {
		options = XPC_VME_ATYPE_A32 | XPC_VME_DTYPE_2eVME |
		    XPC_VME_PTYPE_USER;
	} else if (KW_BLT_2ESST == a_mode) {
		options = XPC_VME_ATYPE_A32 | XPC_VME_DTYPE_2eSST |
		    XPC_VME_PTYPE_USER;
	} else {
		log_die(LOGL, "Unknown mapping mode %d=%s.", a_mode,
		    keyword_get_string(a_mode));
	}

	sicy_init();
	blt_init();
	private->xpc_addr = CesXpcBridge_MasterMap64(g_xpc_bridge,
	    a_map->address, a_map->bytes, options);
	if (-1 == (CesInt64)private->xpc_addr) {
		log_die(LOGL, "CesXpcBridge_MasterMap64 failed.");
	}
	LOGF(verbose)(LOGL, "xpc_addr=%p",
	    (void *)(intptr_t)private->xpc_addr);
	io_opts = 0;
	if (a_do_fifo) {
		/* TODO: Check CES_DMAEL_FIFO_MODE and CES_DMAEL_BOUNDARY. */
		io_opts = cesDmaElSetOption(io_opts, CES_DMAEL_FIFO_MODE, 1);
	}
	private->chain_el_io_opts = io_opts;
	private->chain = cesDmaNewChain();
	if (NULL == private->chain) {
		log_die(LOGL, "cesDmaNewChain failed.");
	}
	a_map->private = private;
	LOGF(verbose)(LOGL, "blt_map }");
}

int
blt_read(struct Map *a_mapper, size_t a_offset, void *a_target, size_t
    a_bytes, int a_berr_ok)
{
	struct CesDmaChainEl *el;
	struct BLT *private;
	CesError error;
	int ret;

	private = a_mapper->private;
	LOGF(spam)(LOGL, "blt_read(source=0x%08x%08x, offset=0x%"PRIzx", "
	    "target=%p, bytes=0x%"PRIzx") {",
	    (uint32_t)(private->xpc_addr >> 32),
	    (uint32_t)(private->xpc_addr & 0xffffffff), a_offset,
	    a_target, a_bytes);
	ret = -1;

	el = cesDmaChainAdd64(private->chain, private->xpc_addr
	    + a_offset, g_xpc_bus, (uintptr_t)a_target, g_virt_bus, a_bytes,
	    private->chain_el_io_opts);
	error = cesDmaChainElErr(el);
	if (CES_EINVAL == error) {
		log_error(LOGL, "cesDmaChainAdd64: Invalid parameter.");
		goto blt_read_done;
	} else if (CES_EALIGN == error) {
		log_error(LOGL, "cesDmaChainAdd64: Unsupported alignment.");
		goto blt_read_done;
	} else if (CES_EINTERNAL == error) {
		log_error(LOGL, "cesDmaChainAdd64: Internal error.");
		goto blt_read_done;
	} else if (CES_OK != error) {
		log_error(LOGL, "cesDmaChainAdd64: The error %d should not "
		    "occur.", error);
		goto blt_read_done;
	}
	if (a_berr_ok) {
		sigbus_set_ok(1);
	}
	cesDmaChainStart(private->chain);
	error = cesDmaChainWait(private->chain);
	cesDmaChainReset(private->chain);
	if (a_berr_ok) {
		sigbus_set_ok(0);
	}
	if (CES_OK != error) {
		/* BERR might be OK for ending. */
		if (!a_berr_ok ||
		    BLT_ERR_BERR != (BLT_ERROR_MASK & error)) {
			log_error(LOGL, "DMA chain read failed: %s (%08x).",
			    cesError2String(0xffff & error), error);
			goto blt_read_done;
		}
	}
	ret = a_bytes;

blt_read_done:
	LOGF(spam)(LOGL, "blt_read(bytes=%d) }", ret);
	return ret;
}

MAP_FUNC_EMPTY(blt_setup);
MAP_FUNC_EMPTY(blt_shutdown);

void
blt_unmap(struct Map *a_map)
{
	struct BLT *private;

	LOGF(debug)(LOGL, "blt_unmap {");
	private = a_map->private;
	cesDmaElSetOption(private->chain_el_io_opts, CES_DMAEL_DESTROYOPTION,
	    0);
	cesDmaChainDelete(private->chain);
	CesXpcBridge_MasterUnMap64(g_xpc_bridge, private->xpc_addr,
	    a_map->bytes);
	FREE(a_map->private);
	LOGF(debug)(LOGL, "blt_unmap }");
}

struct MapBltDst *
map_blt_dst_alloc(size_t a_bytes)
{
	struct MapBltDst *dst;

	CALLOC(dst, 1);
	dst->fd = open("/tmp/shm/test", O_RDWR | O_CREAT, 0644);
	if (-1 == dst->fd) {
		err_(EXIT_FAILURE, "open");
	}
	if (-1 == ftruncate(dst->fd, a_bytes)) {
		err_(EXIT_FAILURE, "ftruncate");
	}
	dst->ptr = mmap(NULL, a_bytes, PROT_READ | PROT_WRITE, MAP_SHARED,
	    dst->fd, 0);
	if (MAP_FAILED == dst->ptr) {
		err_(EXIT_FAILURE, "mmap");
	}
	dst->bytes = a_bytes;
	return dst;
}

void
map_blt_dst_free(struct MapBltDst **a_dst)
{
	struct MapBltDst *dst;

	dst = *a_dst;
	if (NULL != dst) {
		munmap(dst->ptr, dst->bytes);
		close(dst->fd);
		FREE(*a_dst);
	}
}

void *
map_blt_dst_get(struct MapBltDst *a_dst)
{
	return a_dst->ptr;
}

#endif
