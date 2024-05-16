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

#include <module/map/map.h>
#include <module/map/internal.h>
#include <assert.h>
#include <signal.h>
#include <nurdlib/base.h>
#include <nurdlib/log.h>
#include <nurdlib/serialio.h>
#include <util/fmtmod.h>
#include <util/queue.h>
#include <util/string.h>

#if NCONF_mMAP_bRIO4_XPC_3_2_6

#	include <sys/ioctl.h>
#	include <sys/mman.h>
#	include <errno.h>
#	include <fcntl.h>
#	include <unistd.h>
#	include <ces/xpc_vme.h>
#	include <ces/xpc_dma.h>
#	define MAP_VME_DEVICE_STR "/dev/xpc_vme"
#	define MAP_DMA_DEVICE_STR "/dev/xpc_dma"

struct BLT {
	uint32_t	xpc_address;
	xpc_dma_opts_e	options;
};

static void	init(void);

static int g_map_vme_fd = -1, g_map_dma_fd = -1;

void
poke_r(uint32_t a_address, uintptr_t a_ofs_r, unsigned a_bits_r)
{
	xpc_vme_safe_pio_t pio;

	/*
	 * If this fails, even only once, mapping is unusable until
	 * the next reboot, but at least it does not freeze so one can
	 * reboot remotely...
	 */
	pio.vme_address = a_address + a_ofs_r;
	pio.access_type = XPC_VME_A32_STD_USER;
	pio.write_access = 0;
	pio.data_size = a_bits_r / 8;
	/* TODO: Is this still actually avoiding lockups? */
	if (-1 == ioctl(g_map_vme_fd, XPC_VME_SAFE_PIO, &pio)) {
		LOGF(info)(LOGL, "Poke reading ioctl(0x%08x+0x%"PRIpx"(%u)) "
		    "failed, wrong address?", a_address, a_ofs_r,
		    a_bits_r);
		log_die(LOGL, "Note that you have to reboot your "
		    "DAQ computer now!");
	}
}

void
poke_w(uint32_t a_address, uintptr_t a_ofs_w, unsigned a_bits_w, uint32_t
    a_value)
{
	/* Cannot do the write poke. */
	(void)a_address;
	(void)a_ofs_w;
	(void)a_bits_w;
	(void)a_value;
}

void
blt_deinit(void)
{
	LOGF(verbose)(LOGL, "blt_deinit {");
	if (-1 == g_map_dma_fd) {
		close(g_map_dma_fd);
		g_map_dma_fd = -1;
	}
	if (-1 == g_map_vme_fd) {
		close(g_map_vme_fd);
		g_map_vme_fd = -1;
	}
	LOGF(verbose)(LOGL, "blt_deinit }");
}

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int
    a_do_mblt_swap)
{
	xpc_master_map_t map;
	int channel;
	struct BLT *private;

	(void) a_do_mblt_swap;

	LOGF(verbose)(LOGL, "blt_map (blt_xpc_3_2_6) {");
	init();
	CALLOC(private, 1);
	if (KW_BLT == a_mode) {
		private->options = XPC_DMA_READ | XPC_DMA_USER_BUFFER;
	} else if (KW_MBLT == a_mode) {
		private->options = XPC_DMA_READ | XPC_DMA_USER_BUFFER
		    | XPC_DMA_VME_BURST_512;
	} else if (KW_BLT_2ESST == a_mode) {
		private->options = XPC_DMA_READ | XPC_DMA_USER_BUFFER
		    | XPC_DMA_VME_BURST_1024;
	} else {
		log_die(LOGL, "Unknown mapping mode %d=%s.", a_mode,
		    keyword_get_string(a_mode));
	}

	if (a_do_fifo) {
		private->options |= XPC_DMA_VME_ADDR_CONST;
	}

	map.bus_address = a_map->address;
	map.size = a_map->bytes;
	if (KW_BLT == a_mode) {
		map.access_type = XPC_VME_A32_BLT_USER;
	} else if (KW_MBLT == a_mode) {
		map.access_type = XPC_VME_A32_MBLT_USER;
	} else if (KW_BLT_2ESST == a_mode) {
		map.access_type = XPC_VME_ATYPE_A32 |
		    XPC_VME_DTYPE_2eSST | XPC_VME_PTYPE_USER |
		    XPC_VME_FREQ_SST320;
	} else {
		log_die(LOGL, "This should never happen...");
	}

	if (0 > ioctl(g_map_vme_fd, XPC_MASTER_MAP, &map)) {
		log_die(LOGL, "ioctl() XPC_MASTER_MAP failed: %s",
		    strerror(errno));
	}
	private->xpc_address = map.xpc_address;
	a_map->private = private;

	channel = 0;
	if (0 > ioctl(g_map_dma_fd, XPC_DMA_CHAN_MODIFY, &channel)) {
		log_die(LOGL, "ioctl XPC_DMA_CHAN_MODIFY failed: %s.",
		    strerror(errno));
	}
	LOGF(verbose)(LOGL, "Using DMA channel %d.", channel);

	LOGF(verbose)(LOGL, "blt_map(xpc_address=0x%08x) }",
	    private->xpc_address);
}

MAP_FUNC_EMPTY(blt_setup);
MAP_FUNC_EMPTY(blt_shutdown);

void
blt_unmap(struct Map *a_mapper)
{
	xpc_master_map_t map;
	struct BLT *private;

	LOGF(debug)(LOGL, "blt_unmap {");
	private = a_mapper->private;
	map.xpc_address = private->xpc_address;
	map.size = a_mapper->bytes;
	if (0 < ioctl(g_map_vme_fd, XPC_MASTER_UNMAP, &map)) {
		log_die(LOGL, "ioctl(addr=%08x,bytes=%"PRIzx") "
		    "XPC_MASTER_UNMAP: %s.", private->xpc_address,
		    a_mapper->bytes, strerror(errno));
	}
	FREE(a_mapper->private);
	LOGF(debug)(LOGL, "blt_unmap }");
}

int
blt_read(struct Map *a_mapper, size_t a_offset, void *a_target, size_t
    a_bytes, int a_berr_ok)
{
	struct BLT *private;
	xpc_dma_t map;
	int ret;

	if (a_berr_ok) {
		log_die(LOGL, "XPC 3.2.6 does not return BLT size, so cannot "
		    "finish on BERR!");
	}

	private = a_mapper->private;
	map.mem_address = (uintptr_t)a_target;
	map.xpc_address = private->xpc_address + a_offset;
	map.size = a_bytes;
	map.options = private->options;

	ret = ioctl(g_map_dma_fd, XPC_DMA_EXEC, &map);
	if (0 > ret) {
		log_error(LOGL, "XPC_DMA_EXEC: %d=%s.", errno,
		    strerror(errno));
		ret = -1;
	} else {
		/*
		 * DMA call does not return the number of bytes read, so
		 * return the number of bytes successfully requested.
		 */
		ret = a_bytes;
	}
	return ret;
}

void
init(void)
{
	LOGF(verbose)(LOGL, "init {");
	if (-1 == g_map_vme_fd) {
		g_map_vme_fd = open(MAP_VME_DEVICE_STR, O_RDWR);
		if (-1 == g_map_vme_fd) {
			log_die(LOGL, "open("MAP_VME_DEVICE_STR"): %s.",
			    strerror(errno));
		}
		LOGF(verbose)(LOGL, "g_map_vme_fd = %d.", g_map_vme_fd);
	}
	if (-1 == g_map_dma_fd) {
		g_map_dma_fd = open(MAP_DMA_DEVICE_STR, O_RDWR);
		if (-1 == g_map_dma_fd) {
			log_die(LOGL, "open("MAP_DMA_DEVICE_STR"): %s.",
			    strerror(errno));
		}
		LOGF(verbose)(LOGL, "g_map_dma_fd = %d.", g_map_dma_fd);
	}
	LOGF(verbose)(LOGL, "init }");
}

/* map_blt_dst_* handled by blt_dumb */

#endif
