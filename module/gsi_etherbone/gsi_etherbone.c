/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2024
 * Michael Munch
 * Stephane Pietri
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

#include <module/gsi_etherbone/internal.h>
#include <errno.h>
#include <unistd.h>
#include <module/gsi_etherbone/offsets.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/serialio.h>
#include <module/map/map.h>
#include <util/bits.h>

#define NAME "Gsi Etherbone"

#if !NCONF_mGSI_ETHERBONE_bNO

#include <sys/mman.h>
#include <fcntl.h>

#	include <gsi_tm_latch.h>

/* TODO: Make these configs. */
#	define WR_DEVICE_NAME	"dev/wbm0"
#	define PEXARIA_DEV_NAME "/dev/pcie_wb0"

/*
 * Get with:
 * eb-ls dev/wbm0 | grep GSI_TM_LATCH_V2 | awk '{print $3}'
 * Must be done before switching to direct mode!
 */
#	define PEXARIA_TLU_DIRECT_ENABLE 0x4000100
#	define VETAR_TLU_DIRECT_ENABLE   0x2000100
/* Must be 4294967295u, -1 or hex won't work! */
#	define TLU_DIRECT_DISABLE 4294967295u

#	define DIRECT_READ(field) \
    (KW_GSI_VETAR == a_etherbone->module.type ? \
     MAP_READ(a_etherbone->direct.vetar_sicy_map, field) : \
     a_etherbone->direct.pexaria_mmap[OFS_##field/4])

#	define DIRECT_WRITE(field, val) do { \
	if (KW_GSI_VETAR == a_etherbone->module.type) { \
		MAP_WRITE(a_etherbone->direct.vetar_sicy_map, field, val); \
	} else { \
		a_etherbone->direct.pexaria_mmap[OFS_##field/4] = val; \
	} \
} while (0)

static void	direct_create(struct GsiEtherboneModule *, enum Keyword);
static void	direct_deinit(struct GsiEtherboneModule *);
static int	direct_init(struct GsiEtherboneModule *) FUNC_RETURNS;
static uint32_t	direct_readout(struct GsiEtherboneModule *, struct
    EventBuffer *) FUNC_RETURNS;
static uint32_t	direct_readout_dt(struct GsiEtherboneModule *) FUNC_RETURNS;
static uint32_t	direct_stat(struct GsiEtherboneModule *, uint32_t *)
	FUNC_RETURNS;
static void	etherbone_deinit(void);
static int	etherbone_init(struct GsiEtherboneModule *) FUNC_RETURNS;
static uint32_t	etherbone_readout(struct GsiEtherboneModule *, struct
    EventBuffer *) FUNC_RETURNS;
static uint32_t	etherbone_readout_dt(struct GsiEtherboneModule *)
	FUNC_RETURNS;
static uint32_t	etherbone_stat(struct GsiEtherboneModule *, uint32_t *)
	FUNC_RETURNS;
static uint32_t	write_ts(struct EventBuffer *, uint32_t, uint32_t, uint32_t)
	FUNC_RETURNS;

static eb_socket_t g_eb_socket = EB_NULL;
static eb_device_t g_eb_device = EB_NULL;

void
direct_create(struct GsiEtherboneModule *a_etherbone, enum Keyword a_type)
{
	LOGF(verbose)(LOGL, NAME" direct_create {");
	if (KW_GSI_VETAR == a_type) {
		a_etherbone->direct.vetar_sicy_map = NULL;
	} else {
		a_etherbone->direct.pexaria_mmap = MAP_FAILED;
	}
	LOGF(verbose)(LOGL, NAME" direct_create }");
}

void
direct_deinit(struct GsiEtherboneModule *a_etherbone)
{
	LOGF(info)(LOGL, NAME" direct_deinit {");
	if (KW_GSI_VETAR == a_etherbone->module.type) {
		map_unmap(&a_etherbone->direct.vetar_sicy_map);
	} else if (MAP_FAILED != a_etherbone->direct.pexaria_mmap) {
		if (0 != munmap(a_etherbone->direct.pexaria_mmap, MAP_SIZE)) {
			log_error(LOGL, "munmap: %s.", strerror(errno));
		}
		a_etherbone->direct.pexaria_mmap = MAP_FAILED;
	}
	LOGF(info)(LOGL, NAME" direct_deinit }");
}

int
direct_init(struct GsiEtherboneModule *a_etherbone)
{
	eb_data_t fifo_size;

	LOGF(info)(LOGL, NAME" direct_init {");

	if (KW_GSI_VETAR == a_etherbone->module.type) {
		a_etherbone->direct.address =
		    config_get_block_param_int32(a_etherbone->module.config,
			0);
		LOGF(info)(LOGL, "Address=0x%08x.",
		    a_etherbone->direct.address);
		a_etherbone->direct.vetar_sicy_map =
		    map_map(a_etherbone->direct.address,
			MAP_SIZE, KW_NOBLT, 0, 0,
			MAP_POKE_REG(fifo_ready),
			MAP_POKE_REG(fifo_clear), 0);
	} else {
		int fd_tlu;

		fd_tlu = open(PEXARIA_DEV_NAME, O_RDWR);
		if (-1 == fd_tlu) {
			log_error(LOGL, "open(%s) failed: %s.",
			    PEXARIA_DEV_NAME, strerror(errno));
		}
		a_etherbone->direct.pexaria_mmap = mmap(NULL,
		    MAP_SIZE,
		    PROT_WRITE | PROT_READ,
		    MAP_SHARED | MAP_LOCKED, fd_tlu, 0);
		if (MAP_FAILED == a_etherbone->direct.pexaria_mmap) {
			log_die(LOGL, "mmap(%s) failed: %s.",
			    PEXARIA_DEV_NAME, strerror(errno));
		}
		if (-1 == close(fd_tlu)) {
			log_die(LOGL, "close(%s) failed: %s.",
			    PEXARIA_DEV_NAME, strerror(errno));
		}
	}
	DIRECT_WRITE(ch_select, a_etherbone->fifo_id);
	fifo_size = DIRECT_READ(chns_fifosize);
	DIRECT_WRITE(fifo_clear, 0xffffffff);
	DIRECT_WRITE(trig_armset, 0xffffffff);

	LOGF(verbose)(LOGL, "FIFO=%u size=%u.",
	    a_etherbone->fifo_id, (uint32_t)fifo_size);

	LOGF(info)(LOGL, NAME" direct_init }");

	return 1;
}

uint32_t
direct_readout(struct GsiEtherboneModule *a_etherbone, struct EventBuffer
    *a_event_buffer)
{
	uint32_t ret = 0;
	unsigned i;

	LOGF(spam)(LOGL, NAME" direct_readout {");

	for (i = 0; i < a_etherbone->fifo_num; ++i) {
		uint32_t hi, lo, fn;

		DIRECT_WRITE(fifo_pop, 0xf);
		SERIALIZE_IO;
		hi = DIRECT_READ(fifo_ftshi);
		lo = DIRECT_READ(fifo_ftslo);
		/* TODO: Skip? */
		fn = DIRECT_READ(fifo_ftssub);
		ret = write_ts(a_event_buffer, hi, lo, fn);
		if (0 != ret) {
			goto direct_readout_done;
		}
	}

direct_readout_done:
	LOGF(spam)(LOGL, NAME" direct_readout }");

	return ret;
}

uint32_t
direct_readout_dt(struct GsiEtherboneModule *a_etherbone)
{
	a_etherbone->fifo_num = DIRECT_READ(fifo_cnt);
	return 0;
}

uint32_t
direct_stat(struct GsiEtherboneModule *a_etherbone, uint32_t *a_stat)
{
	*a_stat = DIRECT_READ(fifo_ready);
	return 0;
}

#define EB_CALL(func, args, msg, label) do {\
		eb_status_t ret_;\
		ret_ = func args;\
		if (EB_OK != ret_) {\
			log_error(LOGL, #func": "msg" failed: %s.",\
			    eb_status(ret_));\
			goto label;\
		}\
	} while (0)

void
etherbone_deinit(void)
{
	LOGF(info)(LOGL, NAME" etherbone_deinit {");
	if (EB_NULL != g_eb_device) {
		EB_CALL(eb_device_close, (g_eb_device), "Closing device",
		    device_close);
device_close:
		g_eb_device = EB_NULL;
	}
	if (EB_NULL != g_eb_socket) {
		EB_CALL(eb_socket_close, (g_eb_socket), "Closing socket",
		    socket_close);
socket_close:
		g_eb_socket = EB_NULL;
	}
	LOGF(info)(LOGL, NAME" etherbone_deinit }");
}

int
etherbone_init(struct GsiEtherboneModule *a_etherbone)
{
	struct sdb_device sdb_dev;
	eb_data_t fifo_ch;
	eb_data_t fifo_size;
	int device_num, ret = 0;

	LOGF(info)(LOGL, NAME" etherbone_init {");

	if (EB_NULL != g_eb_socket ||
	    EB_NULL != g_eb_device) {
		log_die(LOGL, "Several Etherbone devices not supported!");
	}

	EB_CALL(eb_socket_open, (EB_ABI_CODE, 0, EB_ADDR32 | EB_DATA32,
	    &g_eb_socket),
	    "Opening socket", etherbone_init_fail);
	EB_CALL(eb_device_open, (g_eb_socket, WR_DEVICE_NAME, EB_ADDR32 |
	    EB_DATA32, 3, &g_eb_device),
	    "Opening device", etherbone_init_fail);

	device_num = 1;
	EB_CALL(eb_sdb_find_by_identity, (g_eb_device, GSI_TM_LATCH_VENDOR,
	    GSI_TM_LATCH_PRODUCT, &sdb_dev, &device_num),
	    "Finding HW", etherbone_init_fail);
	LOGF(info)(LOGL, "ABI class = 0x%04x.", sdb_dev.abi_class);
	LOGF(info)(LOGL, "ABI version = 0x%02x.%02x.",
	    sdb_dev.abi_ver_major, sdb_dev.abi_ver_minor);
	LOGF(info)(LOGL, "Name = %s.", sdb_dev.sdb_component.product.name);
	if (1 != device_num) {
		log_error(LOGL, "Found %u Etherbone TLUs, should be 1!",
		    device_num);
		goto etherbone_init_fail;
	}

	a_etherbone->etherbone.tlu_address =
	    sdb_dev.sdb_component.addr_first;
	LOGF(info)(LOGL, "TLU address = 0x%"PRIpx"\n",
	    a_etherbone->etherbone.tlu_address);

	EB_CALL(eb_device_write, (g_eb_device,
	    a_etherbone->etherbone.tlu_address + GSI_TM_LATCH_CH_SELECT,
	    EB_BIG_ENDIAN | EB_DATA32, a_etherbone->fifo_id, 0, eb_block),
	    "Selecting FIFO", etherbone_init_fail);
	EB_CALL(eb_device_read, (g_eb_device,
	    a_etherbone->etherbone.tlu_address + GSI_TM_LATCH_CH_SELECT,
	    EB_BIG_ENDIAN | EB_DATA32, &fifo_ch, 0, eb_block),
	    "Querying FIFO channel", etherbone_init_fail);
	LOGF(verbose)(LOGL, "Etherbone FIFO channel = %u", (uint32_t)fifo_ch);
	if (fifo_ch != a_etherbone->fifo_id) {
		log_error(LOGL, "Set FIFO %u, read back %u!",
		    a_etherbone->fifo_id, (unsigned)fifo_ch);
		goto etherbone_init_fail;
	}

	EB_CALL(eb_device_read, (g_eb_device,
	    a_etherbone->etherbone.tlu_address + GSI_TM_LATCH_CHNS_FIFOSIZE,
	    EB_BIG_ENDIAN | EB_DATA32, &fifo_size, 0, eb_block),
	    "Querying FIFO size", etherbone_init_fail);
	LOGF(verbose)(LOGL, "Etherbone FIFO size = %u", (uint32_t)fifo_size);

	EB_CALL(eb_device_write, (g_eb_device,
	    a_etherbone->etherbone.tlu_address + GSI_TM_LATCH_FIFO_CLEAR,
	    EB_BIG_ENDIAN | EB_DATA32, 0xFFFFFFFF, 0, eb_block),
	    "Clearing FIFO", etherbone_init_fail);
	EB_CALL(eb_device_write, (g_eb_device,
	    a_etherbone->etherbone.tlu_address + GSI_TM_LATCH_TRIG_ARMSET,
	    EB_BIG_ENDIAN | EB_DATA32, 0xFFFFFFFF, 0, eb_block),
	    "Arming", etherbone_init_fail);

	ret = 1;

etherbone_init_fail:
	LOGF(info)(LOGL, NAME" etherbone_init }");
	return ret;
}

uint32_t
etherbone_readout(struct GsiEtherboneModule *a_etherbone, struct EventBuffer
    *a_event_buffer)
{
	eb_cycle_t eb_cycle;
	unsigned i;
	int ret;

	LOGF(spam)(LOGL, NAME" etherbone_readout {");
	ret = CRATE_READOUT_FAIL_ERROR_DRIVER;

	EB_CALL(eb_cycle_open, (g_eb_device, 0, eb_block, &eb_cycle),
	    "Cycle open", etherbone_readout_done);
	for (i = 0; i < a_etherbone->fifo_num; ++i) {
		eb_data_t hi, lo, fn;

		eb_cycle_write(eb_cycle, a_etherbone->etherbone.tlu_address +
		    GSI_TM_LATCH_FIFO_POP, EB_BIG_ENDIAN | EB_DATA32, 0xF);

		eb_cycle_read(eb_cycle, a_etherbone->etherbone.tlu_address +
		    GSI_TM_LATCH_FIFO_FTSHI, EB_BIG_ENDIAN | EB_DATA32, &hi);
		eb_cycle_read(eb_cycle, a_etherbone->etherbone.tlu_address +
		    GSI_TM_LATCH_FIFO_FTSLO, EB_BIG_ENDIAN | EB_DATA32, &lo);
		eb_cycle_read(eb_cycle, a_etherbone->etherbone.tlu_address +
		    GSI_TM_LATCH_FIFO_FTSSUB, EB_BIG_ENDIAN | EB_DATA32, &fn);

		ret = write_ts(a_event_buffer, hi, lo, fn);
	}
	EB_CALL(eb_cycle_close, (eb_cycle),
	    "Cycle close", etherbone_readout_done);

	ret = 0;

etherbone_readout_done:
	LOGF(spam)(LOGL, NAME" etherbone_readout }");
	return ret;
}

uint32_t
etherbone_readout_dt(struct GsiEtherboneModule *a_etherbone)
{
	uint32_t ret = CRATE_READOUT_FAIL_ERROR_DRIVER;

	EB_CALL(eb_device_read, (g_eb_device,
	    a_etherbone->etherbone.tlu_address + GSI_TM_LATCH_FIFO_CNT,
	    EB_BIG_ENDIAN | EB_DATA32, &a_etherbone->fifo_num, 0, eb_block),
	    "Querying FIFO num", etherbone_readout_dt_fail);
	ret = 0;
etherbone_readout_dt_fail:
	return ret;
}

uint32_t
etherbone_stat(struct GsiEtherboneModule *a_etherbone, uint32_t *a_stat)
{
	eb_data_t stat;
	uint32_t ret = CRATE_READOUT_FAIL_ERROR_DRIVER;

	EB_CALL(eb_device_read, (g_eb_device,
	    a_etherbone->etherbone.tlu_address + GSI_TM_LATCH_FIFO_READY,
	    EB_BIG_ENDIAN | EB_DATA32, &stat, 0, eb_block),
	    "Querying FIFO ready", etherbone_stat_fail);
	*a_stat = stat;
	ret = 0;
etherbone_stat_fail:
	return ret;
}

uint32_t
gsi_etherbone_check_empty(struct GsiEtherboneModule *a_etherbone)
{
	uint32_t ret, stat;

	stat = 0;
	if (a_etherbone->is_direct) {
		ret = direct_stat(a_etherbone, &stat);
	} else {
		ret = etherbone_stat(a_etherbone, &stat);
	}
	if (0 == ret) {
		uint32_t fifo_mask;

		fifo_mask = 1 << a_etherbone->fifo_id;
		if ((stat & fifo_mask) != 0) {
			log_error(LOGL, "TLU FIFO %u not empty, status=0x%x.",
			    a_etherbone->fifo_id, stat);
			ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		}
	}
	return ret;
}

void
gsi_etherbone_create(struct ConfigBlock *a_block, struct GsiEtherboneModule
    *a_etherbone, enum Keyword a_type)
{
	enum Keyword const c_mode[] = {KW_DIRECT, KW_ETHERBONE, KW_FASTEST};

	LOGF(verbose)(LOGL, NAME" create {");
	switch (a_type) {
	case KW_GSI_PEXARIA: a_etherbone->fifo_id = 0; break;
	case KW_GSI_VETAR: a_etherbone->fifo_id = 3; break;
	default:
		log_die(LOGL, NAME" can not be %s.",
		    keyword_get_string(a_type));
	}
	a_etherbone->mode = CONFIG_GET_KEYWORD(a_block, KW_MODE, c_mode);
	if (KW_DIRECT == a_etherbone->mode) {
		LOGF(verbose)(LOGL, "Mode=Direct required.");
	} else if (KW_ETHERBONE == a_etherbone->mode) {
		LOGF(verbose)(LOGL, "Mode=Etherbone required.");
	} else if (KW_FASTEST == a_etherbone->mode) {
		LOGF(verbose)(LOGL, "Mode=Direct first, Etherbone second.");
	}
	direct_create(a_etherbone, a_type);
	LOGF(verbose)(LOGL, NAME" create }");
}

void
gsi_etherbone_deinit(struct GsiEtherboneModule *a_etherbone)
{
	LOGF(info)(LOGL, NAME" deinit {");
	direct_deinit(a_etherbone);
	etherbone_deinit();
	LOGF(info)(LOGL, NAME" deinit }");
}

int
gsi_etherbone_init_slow(struct GsiEtherboneModule *a_etherbone)
{
	char line[80];
	FILE *dactl;
	char const *dactl_path;
	uint32_t dactl_value, value;
	int ret;

	LOGF(info)(LOGL, NAME" init_slow {");
	ret = 0;
	a_etherbone->is_direct = 0;

	if (KW_GSI_VETAR == a_etherbone->module.type) {
		dactl_path = "/sys/class/vetar/vetar0/dactl";
	} else {
		dactl_path = "/sys/class/pcie_wb/pcie_wb0/dactl";
	}
	if (KW_ETHERBONE == a_etherbone->mode) {
		dactl_value = TLU_DIRECT_DISABLE;
	} else if (KW_GSI_VETAR == a_etherbone->module.type) {
		dactl_value = VETAR_TLU_DIRECT_ENABLE;
	} else {
		dactl_value = PEXARIA_TLU_DIRECT_ENABLE;
	}

	/* TODO: fopen seems to leak, fork writing? */

	/* Read current value. */
	dactl = fopen(dactl_path, "rb");
	if (NULL == dactl) {
		LOGF(info)(LOGL, NAME" fopen(%s) failed: %s, "
		    "skipping direct mode.",
		    dactl_path, strerror(errno));
		/* If dactl doesn't exist, probably direct doesn't. */
		dactl_value = TLU_DIRECT_DISABLE;
		goto dactl_done;
	}
	fgets(line, sizeof line, dactl);
	if (EOF == fclose(dactl)) {
		/* Something is very wrong, let the user sort it out. */
		log_die(LOGL, NAME" fclose(%s) failed: %s.",
		    dactl_path, strerror(errno));
	}
	value = strtol(line, NULL, 0);
	if (value == dactl_value) {
		LOGF(info)(LOGL, NAME" dactl already set.");
		goto dactl_done;
	}

	dactl = fopen(dactl_path, "a");
	if (NULL == dactl) {
		log_error(LOGL, NAME" fopen(%s) failed: %s.",
		    dactl_path, strerror(errno));
		dactl_value = value;
		goto dactl_done;
	}
	if (0 > fprintf(dactl, "%u", dactl_value)) {
		log_error(LOGL, NAME" fprintf(%s) failed: %s.",
		    dactl_path, strerror(errno));
		dactl_value = value;
		goto dactl_done;
	}
	if (EOF == fclose(dactl)) {
		log_die(LOGL, NAME" fclose(%s) failed: %s.",
		    dactl_path, strerror(errno));
	}
dactl_done:

	a_etherbone->is_direct = TLU_DIRECT_DISABLE != dactl_value;
	if (KW_DIRECT == a_etherbone->mode && !a_etherbone->is_direct) {
		log_die(LOGL,
		    "Direct readout required, but enabling it failed.");
	}

	ret = a_etherbone->is_direct ?
	    direct_init(a_etherbone) :
	    etherbone_init(a_etherbone);
	LOGF(info)(LOGL, NAME" init_slow }");
	return ret;
}

uint32_t
gsi_etherbone_readout(struct GsiEtherboneModule *a_etherbone, struct
    EventBuffer *a_event_buffer)
{
	uint32_t ret;
	unsigned event_num;

	LOGF(spam)(LOGL, NAME" readout {");
	ret = 0;
	event_num = MODULE_COUNTER_DIFF(a_etherbone->module);
	if (a_event_buffer->bytes < event_num * sizeof(uint64_t)) {
		log_error(LOGL, NAME" not enough space for timestamp.");
		ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto gsi_etherbone_readout_fail;
	}

	if (a_etherbone->is_direct) {
		ret = direct_readout(a_etherbone, a_event_buffer);
	} else {
		ret = etherbone_readout(a_etherbone, a_event_buffer);
	}
gsi_etherbone_readout_fail:
	LOGF(spam)(LOGL, NAME" readout }");
	return ret;
}

uint32_t
gsi_etherbone_readout_dt(struct GsiEtherboneModule *a_etherbone)
{
	uint32_t result;

	LOGF(spam)(LOGL, NAME" readout_dt {");
	result = 0;

	if (a_etherbone->is_direct) {
		result = direct_readout_dt(a_etherbone);
	} else {
		result = etherbone_readout_dt(a_etherbone);
	}
	LOGF(spam)(LOGL, "fifo_num=%u.", (uint32_t)a_etherbone->fifo_num);
	a_etherbone->module.event_counter.value += a_etherbone->fifo_num;

	LOGF(spam)(LOGL, NAME" readout_dt }");
	return result;
}

uint32_t
write_ts(struct EventBuffer *a_event_buffer, uint32_t a_hi, uint32_t a_lo,
    uint32_t a_fn)
{
	uint32_t *p32;
	uint64_t ts;

	LOGF(spam)(LOGL, "hi=%08x lo=%08x fine=%08x.", a_hi, a_lo, a_fn);

	p32 = a_event_buffer->ptr;
	if (!MEMORY_CHECK(*a_event_buffer, p32 + 1)) {
		return CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	}

	ts =
	    ((uint64_t)a_hi << 35) |
	    ((uint64_t)a_lo << 3) |
	    ((uint64_t)a_fn & 0x7);
	/* Store network-order/big-endian. */
	*p32++ = htonl_(ts >> 32);
	*p32++ = htonl_(ts & 0xffffffff);
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p32);
	return 0;
}

#else

uint32_t
gsi_etherbone_check_empty(struct GsiEtherboneModule *a_etherbone)
{
	(void)a_etherbone;
	LOGF(info)(LOGL, "gsi_etherbone didn't pass nconf!");
	return 0;
}

void
gsi_etherbone_create(struct ConfigBlock *a_block, struct GsiEtherboneModule
    *a_etherbone, enum Keyword a_type)
{
	(void)a_block;
	(void)a_etherbone;
	(void)a_type;
	LOGF(info)(LOGL, "gsi_etherbone didn't pass nconf!");
}

void
gsi_etherbone_deinit(struct GsiEtherboneModule *a_etherbone)
{
	(void)a_etherbone;
	LOGF(info)(LOGL, "gsi_etherbone didn't pass nconf!");
}

int
gsi_etherbone_init_slow(struct GsiEtherboneModule *a_etherbone)
{
	(void)a_etherbone;
	LOGF(info)(LOGL, "gsi_etherbone didn't pass nconf!");
	return 0;
}

uint32_t
gsi_etherbone_readout(struct GsiEtherboneModule *a_etherbone, struct
    EventBuffer *a_event_buffer)
{
	(void)a_etherbone;
	(void)a_event_buffer;
	LOGF(info)(LOGL, "gsi_etherbone didn't pass nconf!");
	return 0;
}

uint32_t
gsi_etherbone_readout_dt(struct GsiEtherboneModule *a_etherbone)
{
	(void)a_etherbone;
	LOGF(info)(LOGL, "gsi_etherbone didn't pass nconf!");
	return 0;
}

#endif
