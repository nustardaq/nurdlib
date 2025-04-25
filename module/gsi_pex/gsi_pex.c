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

#include <module/gsi_pex/internal.h>
#include <nurdlib/gsi_pex.h>
#if !NCONF_mGSI_PEX_bNO
#	include <sys/ioctl.h>
#	include <assert.h>
#	include <errno.h>
#	include <sched.h>
#	include <module/gsi_pex/offsets.h>
#	include <nurdlib/config.h>
#	include <nurdlib/crate.h>
#	include <nurdlib/log.h>
#	include <util/string.h>
#	include <util/time.h>

#	define NAME "Gsi Pex"

#	define PEX_MEM_OFS       0x00100000
#	define PEX_DMA_OFS       0x00020000
#	define PEX_SFP_OFS       0x00040000
#	define PEX_INI_REQ     0x344
#	define PEX_PT_AD_R_REP 0x044
#	define PEX_PT_AD_R_REQ 0x240
#	define PEX_PT_AD_W_REP 0x440
#	define PEX_PT_AD_W_REQ 0x644
#	define PEX_PT_TK_R_REQ 0xA11

static int	rx(struct GsiPex *, size_t,  uint32_t *, uint32_t *,
    uint32_t *) FUNC_NONNULL((1, 3)) FUNC_RETURNS;
static void	rx_clear_ch(struct GsiPex *, size_t) FUNC_NONNULL(());
static void	rx_clear_sfps(struct GsiPex *, unsigned) FUNC_NONNULL(());
static void	tx(struct GsiPex *, uint32_t, uint32_t, uint32_t)
	FUNC_NONNULL(());

int
gsi_pex_buf_get(struct GsiPex const *a_pex, struct EventBuffer *a_eb,
    uintptr_t *a_physical_minus_virtual)
{
	if (0 == a_pex->buf_bytes) {
		*a_physical_minus_virtual = a_pex->physical_minus_virtual;
		return 0;
	}
	a_eb->bytes = a_pex->buf_bytes;
	a_eb->ptr = a_pex->buf;
	*a_physical_minus_virtual = a_pex->buf_physical_minus_virtual;
	return 1;
}

struct GsiPex *
gsi_pex_create(struct ConfigBlock *a_block)
{
	struct GsiPex *pex;

	LOGF(info)(LOGL, NAME" create {");
	CALLOC(pex, 1);
	pex->is_v5 = config_get_boolean(a_block, KW_PEX_V5);
	LOGF(info)(LOGL, NAME" create }");
	return pex;
}

void
gsi_pex_deinit(struct GsiPex *a_pex)
{
	if (NULL != a_pex->buf) {
		munmap(a_pex->buf, a_pex->buf_bytes);
		a_pex->buf = NULL;
	}
	if (NULL != a_pex->bar0) {
		munmap(a_pex->bar0, PCI_BAR0_SIZE);
		a_pex->bar0 = NULL;
	}
}

void
gsi_pex_free(struct GsiPex **a_pex)
{
	LOGF(info)(LOGL, NAME" free {");
	FREE(*a_pex);
	LOGF(info)(LOGL, NAME" free }");
}

void
gsi_pex_init(struct GsiPex *a_pex, struct ConfigBlock *a_block)
{
	void *buf = NULL;
	uint8_t *bar0;
	uintptr_t buf_ofs_lo, buf_ofs_hi, buf_ofs, buf_bytes;
	size_t i;
	int fd;

	LOGF(info)(LOGL, NAME" init(0x%x) {", a_pex->sfp_bitmask);

	buf_ofs_lo = config_get_uint32(a_block, KW_BUF_OFS, CONFIG_UNIT_NONE,
	    0, UINT32_MAX);
	buf_ofs_hi = config_get_uint32(a_block, KW_BUF_OFS_HI,
	    CONFIG_UNIT_NONE, 0, UINT32_MAX);
	buf_bytes = config_get_uint32(a_block, KW_BUF_BYTES, CONFIG_UNIT_NONE,
	    0, UINT32_MAX);

	buf_ofs = (uintptr_t)((uint64_t)buf_ofs_hi << 32 | buf_ofs_lo);

	a_pex->is_parallel = config_get_boolean(a_block, KW_PEX_PARALLEL);
	a_pex->token_mode = config_get_boolean(a_block, KW_PEX_WAIT) ? 2 : 0;

	/* Set to 1, 'prepare' flips it so we'll actually start with 0. */
	a_pex->buf_idx = 1;

	/* goc -z */
	if (0) {
		fd = open(PEX_DEV, O_RDWR);
		if (-1 == fd) {
			log_die(LOGL, "open("PEX_DEV"): %s", strerror(errno));
		}

		if (0 != ioctl(fd, _IO(0xe0, 0))) {
			log_die(LOGL, "ioctl("PEX_DEV"): %s",
			    strerror(errno));
		}
		time_sleep(0.1);
		close(fd);
	}

	fd = open(PEX_DEV, O_RDWR);
	if (-1 == fd) {
		log_die(LOGL, "open("PEX_DEV"): %s", strerror(errno));
	}

	bar0 = mmap(NULL, PCI_BAR0_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED |
	    MAP_LOCKED, fd, 0);
	if (MAP_FAILED == bar0) {
		log_die(LOGL, "mmap("PEX_DEV",ofs=0,bytes=0x%08x): %s",
		    PCI_BAR0_SIZE, strerror(errno));
	}
	LOGF(verbose)(LOGL, "Bar0=%p.", (void *)bar0);

	if (0 != buf_ofs) {
		buf = mmap(NULL, buf_bytes, PROT_WRITE | PROT_READ,
		    MAP_SHARED, fd, buf_ofs);
		if (MAP_FAILED == buf) {
			log_die(LOGL, "mmap("PEX_DEV",ofs=0x%"PRIpx","
			    "bytes=0x%"PRIpx"): %s", buf_ofs, buf_bytes,
			    strerror(errno));
		}
		LOGF(verbose)(LOGL, "Buf=%p (ofs=0x%"PRIpx","
		    "bytes=0x%"PRIpx").", buf, buf_ofs, buf_bytes);
	}

	if (-1 == close(fd)) {
		log_die(LOGL, "close("PEX_DEV"): %s", strerror(errno));
	}

	a_pex->bar0 = bar0;
	a_pex->dma = (void *)(bar0 + PEX_DMA_OFS);
	LOGF(verbose)(LOGL, "DMA=%p.", (void volatile *)a_pex->dma);
	a_pex->buf = buf;
	a_pex->buf_bytes = buf_bytes;
	a_pex->buf_physical_minus_virtual = buf_ofs - (uintptr_t)buf;

	for (i = 0; i < LENGTH(a_pex->sfp); ++i) {
		a_pex->sfp[i].phys = PEX_MEM_OFS + PEX_SFP_OFS * i;
		a_pex->sfp[i].virt = bar0 + a_pex->sfp[i].phys;
		LOGF(verbose)(LOGL, "SFP=%"PRIz": phys=0x%"PRIpx", virt=%p.",
		    i, a_pex->sfp[i].phys, a_pex->sfp[i].virt);
	}

	/* pexheal.sh */
	if (0) {
		uint32_t r;

		GSI_PEX_WRITE(a_pex, rx_rst, 0xf);
		GSI_PEX_WRITE(a_pex, rx_rst, 0x0);
		GSI_PEX_WRITE(a_pex, heal, 0x0);
		sleep(1);
		r = GSI_PEX_READ(a_pex, heal);
		LOGF(info)(LOGL, "Pexheal=0x%08x.", r);
	}

	LOGF(info)(LOGL, NAME" init }");
}

void
gsi_pex_physical_minus_virtual_set(struct GsiPex *a_pex, uintptr_t a_ofs)
{
	a_pex->physical_minus_virtual = a_ofs;
}

void
gsi_pex_readout_prepare(struct GsiPex *a_pex)
{
	a_pex->buf_idx ^= 1;
	LOGF(spam)(LOGL, NAME" readout_prepare(SFP-mask=0x%x,buf=%u) {",
	    a_pex->sfp_bitmask, a_pex->buf_idx);
	if (a_pex->is_parallel) {
		/* Issue data transfer on all tagged SFP:s. */
		rx_clear_sfps(a_pex, a_pex->sfp_bitmask);
		tx(a_pex, PEX_PT_TK_R_REQ | (a_pex->sfp_bitmask << 16),
		    a_pex->buf_idx | a_pex->token_mode, 0);
	}
	LOGF(spam)(LOGL, NAME" readout_prepare }");
}

void
gsi_pex_sfp_tag(struct GsiPex *a_pex, unsigned a_sfp_i)
{
	a_pex->sfp_bitmask |= 1 << a_sfp_i;
}

int
gsi_pex_slave_init(struct GsiPex *a_pex, size_t a_sfp_i, size_t a_slave_num)
{
	uint32_t addr, comm, data;
	unsigned trial;
	int ret;

	LOGF(info)(LOGL, NAME" slave_init(%"PRIz", %"PRIz") {", a_sfp_i,
	    a_slave_num);
	ret = 0;
	assert(4 > a_sfp_i);
	/* 1000 slaves should work for some time, eh? */
	assert(1000 > a_slave_num);
	data = comm = addr = 0;
	for (trial = 0;; ++trial) {
		if (10 == trial) {
			log_error(LOGL, "Initializing SFP=%"PRIz" with "
			    "%"PRIz" slaves (had %d) failed, no reply.",
			    a_sfp_i, a_slave_num, addr);
			goto gsi_pex_init_slave_done;
		}
		rx_clear_ch(a_pex, a_sfp_i);
		tx(a_pex, PEX_INI_REQ | (0x10000 << a_sfp_i), 0,
		    a_slave_num - 1);
		if (rx(a_pex, a_sfp_i, &comm, &addr, &data) &&
		    a_slave_num == addr) {
			break;
		}
		sched_yield();
	}
	ret = 1;
gsi_pex_init_slave_done:
	LOGF(info)(LOGL, NAME" slave_init }");
	return ret;
}

int
gsi_pex_slave_read(struct GsiPex *a_pex, size_t a_sfp_i, size_t a_slave_i,
    unsigned a_slave_ofs, uint32_t *a_val)
{
	uint32_t addr, comm;
	int ret;

	ret = 0;
	LOGF(spam)(LOGL,
	    NAME" slave_read(SFP=%"PRIz",slave=%"PRIz",ofs=0x%08x) {",
	    a_sfp_i, a_slave_i, a_slave_ofs);
	addr = a_slave_ofs + (a_slave_i << 24);
	rx_clear_ch(a_pex, a_sfp_i);
	tx(a_pex, PEX_PT_AD_R_REQ | (0x10000 << a_sfp_i), addr, 0);
	if (!rx(a_pex, a_sfp_i, &comm , &addr, a_val)) {
		goto gsi_pex_slave_read_done;
	}
	if (PEX_PT_AD_R_REP != (0xfff & comm)) {
		log_error(LOGL, "Invalid comm = 0x%08x.", comm);
		goto gsi_pex_slave_read_done;
	}
	if (0 != (0x4000 & comm)) {
		log_error(LOGL, "Packet structure = 0x%08x.", comm);
		goto gsi_pex_slave_read_done;
	}
	ret = 1;
gsi_pex_slave_read_done:
	LOGF(spam)(LOGL, NAME" slave_read(val=0x%08x) }", *a_val);
	return ret;
}

int
gsi_pex_slave_write(struct GsiPex *a_pex, size_t a_sfp_i, size_t
    a_slave_i, unsigned a_slave_ofs, uint32_t a_val)
{
	uint32_t addr, comm, data;
	int ret;

	ret = 0;
	LOGF(spam)(LOGL, NAME" slave_write(SFP=%"PRIz",slave=%"PRIz","
	    "ofs=0x%08x,val=0x%08x) {", a_sfp_i, a_slave_i, a_slave_ofs,
	    a_val);
	addr = a_slave_ofs + (a_slave_i << 24);
	rx_clear_ch(a_pex, a_sfp_i);
	tx(a_pex, PEX_PT_AD_W_REQ | (0x10000 << a_sfp_i), addr, a_val);
	if (!rx(a_pex, a_sfp_i, &comm, &addr, &data)) {
		goto gsi_pex_slave_write_done;
	}
	if (PEX_PT_AD_W_REP != (0xfff & comm)) {
		log_error(LOGL, "Invalid comm = 0x%08x.", comm);
		goto gsi_pex_slave_write_done;
	}
	if (0 != (0x4000 & comm)) {
		log_error(LOGL, "Packet structure = 0x%08x.", comm);
		goto gsi_pex_slave_write_done;
	}
	ret = 1;
gsi_pex_slave_write_done:
	LOGF(spam)(LOGL, NAME" slave_write }");
	return ret;
}

/* Issue data transfer on one SFP. */
void
gsi_pex_token_issue_single(struct GsiPex *a_pex, unsigned a_sfp_i)
{
	LOGF(spam)(LOGL,
	    NAME" token_issue_single(SFP=%u,buf=%u,tokmod=%u) {",
	    a_sfp_i, a_pex->buf_idx, a_pex->token_mode);
	rx_clear_ch(a_pex, a_sfp_i);
	tx(a_pex, PEX_PT_TK_R_REQ | (0x10000 << a_sfp_i), a_pex->buf_idx |
	    a_pex->token_mode, 0);
	LOGF(spam)(LOGL, NAME" token_issue_single }");
}

int
gsi_pex_token_receive(struct GsiPex *a_pex, size_t a_sfp_i, uint32_t
    a_slave_num)
{
	uint32_t buf_idx, comm, mask, slave_num, token_check, token_mode;
	int ret;

	LOGF(spam)(LOGL, NAME" token_receive(SFP=%"PRIz") {", a_sfp_i);
	ret = 0;
	comm = 0;
	token_check = 0;
	slave_num = 0;
	if (!rx(a_pex, a_sfp_i, &comm, &token_check, &slave_num)) {
		log_error(LOGL, NAME " token_receive(SFP=%"PRIz",buf=%u,"
		    "tokmod=%u): " "No reply, missing trigger?", a_sfp_i,
		    a_pex->buf_idx, a_pex->token_mode);
		goto gsi_pex_token_receive_fail;
	}
	buf_idx = 1 & token_check;
	if (buf_idx != a_pex->buf_idx) {
		log_error(LOGL, NAME":SFP=%"PRIz": Buffer mismatch, used=%u "
		    "but received=%u.", a_sfp_i, a_pex->buf_idx, buf_idx);
		goto gsi_pex_token_receive_fail;
	}
	token_mode = 2 & token_check;
	if (a_pex->token_mode != token_mode) {
		log_error(LOGL, NAME":SFP=%"PRIz": Token mode mismatch, "
		    "used=%u but received=%u.", a_sfp_i, a_pex->token_mode,
		    token_mode);
		goto gsi_pex_token_receive_fail;
	}
	mask = a_pex->is_v5 ? 0xf : ~0;
	if (0 != a_slave_num && (a_slave_num & mask) != slave_num) {
		log_error(LOGL, NAME":SFP=%"PRIz": Slave num mismatch, "
		    "have=%u but received=%u.", a_sfp_i, a_slave_num,
		    slave_num);
		goto gsi_pex_token_receive_fail;
	}
	ret = 1;
gsi_pex_token_receive_fail:
	LOGF(spam)(LOGL, NAME" token_receive(comm=0x%08x,token=0x%08x,"
	    "num=0x%08x) }", comm, token_check, slave_num);
	return ret;
}

void
gsi_pex_reset(struct GsiPex *a_pex)
{
	LOGF(verbose)(LOGL, NAME" reset {");
	if (a_pex->is_v5) {
		a_pex->dma->stat = 0x20;
	} else {
		a_pex->dma->stat = 0x0;
	}
	LOGF(verbose)(LOGL, NAME" reset }");
}

int
rx(struct GsiPex *a_pex, size_t a_sfp_i, uint32_t *a_comm, uint32_t *a_addr,
    uint32_t *a_data)
{
	int loop, ret = 0;

	LOGF(spam)(LOGL, NAME" rx(sfp=%"PRIz") {", a_sfp_i);
	for (loop = 0; 1000000 > loop; ++loop) {
		uint32_t comm;

		comm = GSI_PEX_READ(a_pex, rep_statn(a_sfp_i));
		if (0x2000 == (0x3000 & comm)) {
			*a_comm = comm;
			if (a_pex->is_v5 && (comm & 0xfff) == PEX_PT_TK_R_REQ)
			{
				if (NULL != a_addr) {
					*a_addr = (comm & 0xf000000) >> 24;
				}
				if (NULL != a_data) {
					*a_data = (comm & 0xf0000) >> 16;
				}
			} else {
				if (NULL != a_addr) {
					*a_addr = GSI_PEX_READ(a_pex,
					    rep_addrn(a_sfp_i));
				}
				if (NULL != a_data) {
					*a_data = GSI_PEX_READ(a_pex,
					    rep_datan(a_sfp_i));
				}
			}
			ret = 1;
			goto rx_ok;
		}
		sched_yield();
	}
	log_error(LOGL, NAME" rx(SFP=%"PRIz") failed.", a_sfp_i);
rx_ok:
	LOGF(spam)(LOGL, NAME" rx(sfp=%"PRIz",comm=0x%x,addr=0x%x,"
	    "data=0x%x) }", a_sfp_i, *a_comm, *a_addr, NULL == a_data ? 0 :
	    *a_data);
	return ret;
}

void
rx_clear_ch(struct GsiPex *a_pex, size_t a_ch)
{
	LOGF(spam)(LOGL, NAME" rx_clear_ch(%"PRIz") {", a_ch);
	if (a_pex->is_v5) {
		GSI_PEX_WRITE(a_pex, rep_clr, 1 << a_ch);
	} else {
		while (0 != (GSI_PEX_READ(a_pex, rep_statn(a_ch)) & 0xf000) ||
		    0 != (GSI_PEX_READ(a_pex, sfp_tk_stat(a_ch)) & 0xf000)) {
			GSI_PEX_WRITE(a_pex, rep_clr, 1 << a_ch);
		}
	}
	LOGF(spam)(LOGL, NAME" rx_clear_ch(%"PRIz") }", a_ch);
}

void
rx_clear_sfps(struct GsiPex *a_pex, unsigned a_bitmask)
{
	LOGF(spam)(LOGL, NAME" rx_clear_sfps(0x%x) {", a_bitmask);
	if (a_pex->is_v5) {
		GSI_PEX_WRITE(a_pex, rep_clr, a_bitmask);
	} else {
		unsigned mask;

		mask = a_bitmask << 8 | a_bitmask << 4 | a_bitmask;
		while (0 != (GSI_PEX_READ(a_pex, rep_stat) & mask)) {
			GSI_PEX_WRITE(a_pex, rep_clr, mask);
		}
	}
	LOGF(spam)(LOGL, NAME" rx_clear_sfps(0x%x) }", a_bitmask);
}

void
tx(struct GsiPex *a_pex, uint32_t a_comm, uint32_t a_addr, uint32_t a_data)
{
	LOGF(spam)(LOGL, NAME" tx(comm=0x%x,addr=0x%x,data=0x%x) {", a_comm,
	    a_addr, a_data);
	if (a_pex->is_v5 && (a_comm & 0xfff) == PEX_PT_TK_R_REQ) {
		GSI_PEX_WRITE(a_pex, req_comm,
		    a_comm | (a_data & 0xf) << 20 | (a_addr & 0x3) << 24 | 1
		    << 28);
	} else {
		GSI_PEX_WRITE(a_pex, req_addr, a_addr);
		GSI_PEX_WRITE(a_pex, req_data, a_data);
		GSI_PEX_WRITE(a_pex, req_comm, a_comm);
	}
	LOGF(spam)(LOGL, NAME" tx(comm=0x%x,addr=0x%x,data=0x%x) }", a_comm,
	    a_addr, a_data);
}

#else

struct GsiPex *
gsi_pex_create(struct ConfigBlock *a_block)
{
	(void)a_block;
	return NULL;
}

void
gsi_pex_deinit(struct GsiPex *a_pex)
{
	(void)a_pex;
}

void
gsi_pex_init(struct GsiPex *a_pex, struct ConfigBlock *a_block)
{
	(void)a_pex;
	(void)a_block;
}

void
gsi_pex_physical_minus_virtual_set(struct GsiPex *a_pex, uintptr_t a_ofs)
{
	(void)a_pex;
	(void)a_ofs;
}

void
gsi_pex_readout_prepare(struct GsiPex *a_pex)
{
	(void)a_pex;
}

void
gsi_pex_reset(struct GsiPex *a_pex)
{
	(void)a_pex;
}

#endif
