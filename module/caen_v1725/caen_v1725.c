/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2025
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

#include <module/caen_v1725/caen_v1725.h>
#include <module/caen_v1725/internal.h>
#include <sched.h>
#include <module/caen_v1725/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <nurdlib/serialio.h>
#include <util/assert.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/string.h>
#include <util/time.h>

#define NAME "Caen v1725"

#define DMA_FILLER 0x17251725
#define NO_DATA_TIMEOUT 1.0

#define CFG_TRIGGER_OVERLAP   (1 << 1)
#define CFG_TEST_PATTERN      (1 << 3)
#define CFG_SELF_TRIGGER_NEG  (1 << 6)

#define CTL_OPTICAL_INTERRUPT (1 << 3)
#define CTL_BERR_ENABLE       (1 << 4)
#define CTL_ALIGN64           (1 << 5)
#define CTL_ADDRESS_RELOC     (1 << 6)
#define CTL_INTERRUPT_ROAK    (1 << 7)
#define CTL_BLT_EXTENDED      (1 << 8)

#define ACQ_MODE_SW           (0)
#define ACQ_MODE_S_IN         (1)
#define ACQ_MODE_FIRST        (2)
#define ACQ_MODE_LVDS         (3)
#define ACQ_START             (1 <<  2)
#define ACQ_ALL_TRIGGERS      (1 <<  3)
#define ACQ_ALMOST_FULL       (1 <<  5)
#define ACQ_EXTERNAL_CLOCK    (1 <<  6)
#define ACQ_BUSY_IO           (1 <<  8)
#define ACQ_VETO_IO           (1 <<  9)
#define ACQ_START_ON_RISING   (1 << 11)
#define ACQ_INHIBIT_TRG_OUT   (1 << 12)

#define SET_THRESHOLDS(v1725, array) \
    set_thresholds(v1725, array, LENGTH(array))

MODULE_PROTOTYPES(caen_v1725);
static void	set_thresholds(struct CaenV1725Module *, uint16_t const *,
    size_t);

uint32_t
caen_v1725_check_empty(struct Module *a_module)
{
	struct CaenV1725Module *v1725;
	uint32_t events;

	LOGF(spam)(LOGL, NAME" check_empty {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	events = MAP_READ(v1725->sicy_map, event_stored);
	LOGF(spam)(LOGL, NAME" check_empty(events=%u) }", events);
	return events > 0 ? CRATE_READOUT_FAIL_DATA_TOO_MUCH : 0;
}

struct Module *
caen_v1725_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV1725Module *v1725;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(v1725);
	v1725->module.event_max = 1;

	v1725->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", v1725->address);

	v1725->module.event_counter.mask = BITS_MASK_TOP(23);

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)v1725;
}

void
caen_v1725_deinit(struct Module *a_module)
{
	struct CaenV1725Module *v1725;

	LOGF(info)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	map_unmap(&v1725->sicy_map);
	map_unmap(&v1725->dma_map);
	LOGF(info)(LOGL, NAME" deinit }");
}

void
caen_v1725_destroy(struct Module *a_module)
{
	(void)a_module;
}

struct Map *
caen_v1725_get_map(struct Module *a_module)
{
	struct CaenV1725Module *v1725;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return v1725->sicy_map;
}

void
caen_v1725_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature }");
}

int
caen_v1725_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1725Module *v1725;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

	{
		double range[16];
		size_t i;

		CONFIG_GET_DOUBLE_ARRAY(range, v1725->module.config, KW_RANGE,
		    CONFIG_UNIT_V, 0.0, 2.0);
		LOGF(verbose)(LOGL, "Input ranges:");
		for (i = 0; i < LENGTH(range); ++i) {
			uint32_t u32;

			u32 = range[i] <= 0.5;
			LOGF(verbose)(LOGL, " [%"PRIz"]=%fV (0x%08x).",
			    i, u32 ? 0.5 : 2.0, u32);
			MAP_WRITE(v1725->sicy_map, input_dynamic_range(i),
			    u32);
		}
	}
	{
		double pwidth[16];
		size_t i;

		CONFIG_GET_DOUBLE_ARRAY(pwidth, v1725->module.config,
		    KW_PULSE_WIDTH, CONFIG_UNIT_NS, 0.0, 255 * 16.0);
		LOGF(verbose)(LOGL, "Discriminator pulse-widths:");
		for (i = 0; i < LENGTH(pwidth); ++i) {
			uint32_t u32;

			u32 = (pwidth[i] + v1725->period_ns - 1) /
			    v1725->period_ns;
			u32 = MIN(u32, 255);
			LOGF(verbose)(LOGL, " [%"PRIz"]=%uns (0x%08x).",
			    i, u32 * v1725->period_ns, u32);
			MAP_WRITE(v1725->sicy_map, channel_n_pulse_width(i),
			    u32);
		}
	}
	{
		uint16_t thr[16];

		CONFIG_GET_UINT_ARRAY(thr, v1725->module.config, KW_THRESHOLD,
		    CONFIG_UNIT_NONE, 0, BITS_MASK_TOP(13));
		SET_THRESHOLDS(v1725, thr);
	}
	{
		uint8_t logic[8];
		size_t i;

		CONFIG_GET_UINT_ARRAY(logic, v1725->module.config,
		    KW_TRIGGER_METHOD, CONFIG_UNIT_NONE, 0, 3);
		LOGF(verbose)(LOGL, "Self-trigger logic:");
		for (i = 0; i < LENGTH(logic); ++i) {
			uint32_t u32;

			u32 = logic[i];
			LOGF(verbose)(LOGL, " [%"PRIz"]=0x%08x.", i, u32);
			MAP_WRITE(v1725->sicy_map,
			    couple_n_self_trigger_logic(i), u32);
		}
	}
	{
		uint16_t offset[16];
		double init_sleep;
		size_t i;

		CONFIG_GET_INT_ARRAY(offset, v1725->module.config,
		    KW_OFFSET, CONFIG_UNIT_NONE, 0, 0xffff);
		init_sleep = config_get_double(v1725->module.config,
		    KW_INIT_SLEEP, CONFIG_UNIT_S, 0, 10);
		LOGF(verbose)(LOGL, "DC offset (sleep=%fs):", init_sleep);
		for (i = 0; i < LENGTH(offset); ++i) {
			uint32_t u32;

			u32 = offset[i];
			LOGF(verbose)(LOGL, " [%"PRIz"]=0x%08x.", i, u32);
			MAP_WRITE(v1725->sicy_map, dc_offset(i), u32);
		}
		time_sleep(init_sleep);
	}
	{
		enum Keyword c_kw_clock[] = {KW_INTERNAL, KW_EXTERNAL};
		uint32_t acq;

		acq =
		    ACQ_MODE_S_IN |
		    ACQ_START |
		    ACQ_ALL_TRIGGERS |
		    ACQ_ALMOST_FULL |
		    /* ACQ_EXTERNAL_CLOCK | */
		    ACQ_BUSY_IO |
		    /* ACQ_VETO_IO | */
		    /* ACQ_START_ON_RISING | */
		    /* ACQ_INHIBIT_TRG_OUT | */
		    0;
		if (KW_EXTERNAL == CONFIG_GET_KEYWORD(v1725->module.config,
		    KW_CLOCK_INPUT, c_kw_clock)) {
			acq |= ACQ_EXTERNAL_CLOCK;
		}
		LOGF(verbose)(LOGL, "Acquisition control=0x%08x.", acq);
		MAP_WRITE(v1725->sicy_map, acquisition_control, acq);
	}

	{
		enum Keyword const c_kw_trig_in[] =
		    {KW_OFF, KW_LVDS, KW_LEMO};
		uint32_t ch, width, level, in;
		uint32_t u32;

		ch = config_get_bitmask(v1725->module.config,
		    KW_TRIGGER_CHANNEL, 0, 15);
		width = config_get_double(v1725->module.config,
		    KW_MAJORITY_WIDTH, CONFIG_UNIT_NS, 0, 15 *
		    v1725->period_ns);
		level = config_get_int32(v1725->module.config,
		    KW_MAJORITY_LEVEL, CONFIG_UNIT_NONE, 1, 16);
		in = CONFIG_GET_KEYWORD(v1725->module.config,
		    KW_TRIGGER_INPUT, c_kw_trig_in);
		switch (in) {
		case KW_OFF: in = 0; break;
		case KW_LVDS: in = 1 << 29; break;
		case KW_LEMO: in = 1 << 30; break;
		}
		u32 =
		    ch |
		    width << 20 |
		    (level - 1) << 24 |
		    in;
		LOGF(verbose)(LOGL, "Global trigger mask=0x%08x.", u32);
		MAP_WRITE(v1725->sicy_map, global_trigger_mask, u32);
	}
	{
		/* TODO: front_panel_trg_out_gpo_enable_mask. */
	}
	{
		uint32_t post;

		post = config_get_int32(v1725->module.config,
		    KW_SAMPLE_LENGTH, CONFIG_UNIT_NONE, 0, BITS_MASK_TOP(30));
		LOGF(verbose)(LOGL, "Post-trigger=0x%08x.", post);
		MAP_WRITE(v1725->sicy_map, post_trigger, post);
	}
	{
		enum Keyword const c_kw_lemo[] = {KW_NIM, KW_TTL};
		uint8_t lvds[4];
		uint32_t lemo, trg_out;
		uint32_t u32;

		lemo = CONFIG_GET_KEYWORD(v1725->module.config, KW_LEMO,
		    c_kw_lemo) == KW_TTL;
		trg_out = config_get_boolean(v1725->module.config,
		    KW_TRIGGER_OUTPUT);
		CONFIG_GET_INT_ARRAY(lvds, v1725->module.config, KW_LVDS,
		    CONFIG_UNIT_NONE, 0, 1);
		u32 =
		    lemo << 0 |
		    trg_out << 1 |
		    lvds[0] << 2 |
		    lvds[1] << 3 |
		    lvds[2] << 4 |
		    lvds[3] << 5 |
		    0;
		LOGF(verbose)(LOGL, "Front panel IO control=0x%08x.", u32);
		MAP_WRITE(v1725->sicy_map, front_panel_i_o_control, u32);
	}
	{
		v1725->channel_enable = config_get_bitmask(
		    v1725->module.config, KW_CHANNEL_ENABLE, 0, 15);
		LOGF(verbose)(LOGL, "Channel mask=0x%08x.",
		    v1725->channel_enable);
		MAP_WRITE(v1725->sicy_map, channel_enable_mask,
		    v1725->channel_enable);
	}

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v1725_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_MBLT,
		KW_FF,
		KW_NOBLT
	};
	struct CaenV1725Module *v1725;
	uint32_t result;
	uint16_t revision;

	result = 0;
	LOGF(info)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

	v1725->blt_mode = CONFIG_GET_KEYWORD(v1725->module.config,
	    KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "Using BLT mode = %s.",
	    keyword_get_string(v1725->blt_mode));

	v1725->do_berr = config_get_boolean(v1725->module.config, KW_BERR);
	LOGF(verbose)(LOGL, "Do BERR BLT=%s.", v1725->do_berr ? "yes" : "no");

	v1725->do_blt_ext = config_get_boolean(v1725->module.config,
	    KW_BLT_EXT);
	LOGF(verbose)(LOGL, "Do extended BLT=%s.",
	    v1725->do_berr ? "yes" : "no");

	v1725->sicy_map = map_map(v1725->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(scratch), MAP_POKE_REG(scratch), 0);

	/* TODO: Check geo before reset! */

	{
		uint32_t ff;

		ff = MAP_READ(v1725->sicy_map,
		    configuration_rom_board_form_factor);
		if (0 == ff) {
			/* VME64, we can write the nurdlib ID. */
			MAP_WRITE(v1725->sicy_map, board_id, v1725->geo);
		}
		v1725->geo = 0x1f & MAP_READ(v1725->sicy_map, board_id);
		LOGF(verbose)(LOGL, "GEO = %u.", v1725->geo);
		if (1 == ff) {
			/* VME64x, we have to remap the nurdlib ID. */
			crate_module_remap_id(a_crate, v1725->module.id,
			    v1725->geo);
		}
	}

	/* Reset, after GEO. */
	MAP_WRITE(v1725->sicy_map, software_reset, 1);
	MAP_WRITE(v1725->sicy_map, software_clear, 1);
	time_sleep(0.1);

	{
		uint32_t u32;

		u32 = CFG_TRIGGER_OVERLAP;
		LOGF(verbose)(LOGL, "Board configuration = 0x%08x.", u32);
		MAP_WRITE(v1725->sicy_map, board_configuration_bit_set, u32);
		MAP_WRITE(v1725->sicy_map, board_configuration_bit_clear,
		    ~u32);
	}

	SERIALIZE_IO;

	{
		unsigned i;

		for (i = 0; i < 16; ++i) {
			revision = MAP_READ(v1725->sicy_map,
			    amc_firmware_revision(i));
			LOGF(info)(LOGL,
			    "AMC Firmware revision = %d.%d (0x%04x).",
			    (0x0000ff00 & revision) >> 8,
			    0x000000ff & revision,
			    revision);
		}
	}

	revision = MAP_READ(v1725->sicy_map, roc_fpga_firmware_revision);
	LOGF(info)(LOGL, "ROC FPGA Firmware revision = %d.%d (0x%04x).",
	    (0x0000ff00 & revision) >> 8, 0x000000ff & revision, revision);

	/* Get module type and channel #. */
	{
		char const *ch_mem_size;
		uint32_t info;

		info = MAP_READ(v1725->sicy_map, board_info);
		switch (info & 0xff) {
		case 0x0e: v1725->period_ns = 16; break;
		case 0x0b: v1725->period_ns = 8; break;
		default:
			log_error(LOGL,
			    "Invalid family-code in board-info 0x%08x.",
			    info);
			goto v1725_init_slow_done;
		}
		switch ((info >> 8) & 0xff) {
		case 0x01: ch_mem_size = "640 kS"; break;
		case 0x08: ch_mem_size = "5.12 MS"; break;
		default:
			log_error(LOGL,
			    "Invalid ch-mem-size in board-info 0x%08x.",
			    info);
			goto v1725_init_slow_done;
		}
		LOGF(verbose)(LOGL, "Ch-mem-size = %s.", ch_mem_size);
		switch ((info >> 16) & 0xff) {
		case 0x10: v1725->ch_num = 16; break;
		case 0x08: v1725->ch_num = 8; break;
		default:
			log_error(LOGL,
			    "Invalid ch-num in board-info 0x%08x.", info);
			goto v1725_init_slow_done;
		}
	}

	/* Enable BERR/SIGBUS to end DMA readout. */
	MAP_WRITE(v1725->sicy_map, readout_control,
	    (v1725->do_berr ? CTL_BERR_ENABLE : 0) |
	    CTL_ALIGN64 |
	    (v1725->do_blt_ext ? CTL_BLT_EXTENDED : 0));

	if (KW_NOBLT != v1725->blt_mode) {
		v1725->dma_map = map_map(v1725->address, 0x1000,
		    v1725->blt_mode, 1, 0,
		    MAP_POKE_REG(scratch), MAP_POKE_REG(scratch), 0);
	}

	result = 1;
v1725_init_slow_done:

	LOGF(info)(LOGL, NAME" init_slow(result=%d) }", result);
	return result;
}

void
caen_v1725_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v1725_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

uint32_t
caen_v1725_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV1725Module *v1725;
	uint32_t *p32;
	uint32_t i, event_size;
	uint32_t result;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");
	result = 0;
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

	p32 = a_event_buffer->ptr;

	event_size = MAP_READ(v1725->sicy_map, event_size);
	LOGF(spam)(LOGL, "Event_size = %u.", event_size);
	if (0) if (0 == event_size) {
		log_error(LOGL, "Event buffer empty!");
		result = CRATE_READOUT_FAIL_DATA_MISSING;
		goto caen_v1725_readout_fail;
	}

	/* Move data from module. */
	if (KW_NOBLT == v1725->blt_mode) {
		for (i = 0; i < event_size; ++i) {
			uint32_t u32;

			u32 = MAP_READ(v1725->sicy_map, event_readout_buffer);
			*p32++ = u32;
		}
	} else {
		size_t bytes;
		int ret;

		bytes = event_size * sizeof(uint32_t);
		p32 = map_align(p32, &bytes, v1725->blt_mode, DMA_FILLER);
		ret = map_blt_read_berr(v1725->dma_map, 0, p32, bytes);
		if (-1 == ret) {
			log_error(LOGL, "DMA read failed!");
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto caen_v1725_readout_fail;
		} else if (bytes == (size_t) ret) {
			/* Got all data that was promised. */
			p32 += bytes / sizeof(uint32_t);
		} else if (0 == ret) {
			/* The V1725 sometimes lies about event_size.
			 * Looks like it has the previous event_size, but
			 * there is no new data available.
			 * We accept and treat that as no data available.
			 */
			/* printf ("0 read from v1725.\n"); */
		} else {
			/* Short read.  Not expected. */
			log_error(LOGL, "Partial DMA read - unexpected!");
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto caen_v1725_readout_fail;
		}
	}

caen_v1725_readout_fail:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p32);
	LOGF(spam)(LOGL, NAME" readout }");
	return result;
}

uint32_t
caen_v1725_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1725Module *v1725;
	uint32_t event_size;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	event_size = MAP_READ(v1725->sicy_map, event_size);
	LOGF(spam)(LOGL, NAME" readout_dt(event_size=%u) }", event_size);
	return 0;
}

void
caen_v1725_setup_(void)
{
	MODULE_SETUP(caen_v1725, 0);
}

void
set_thresholds(struct CaenV1725Module *a_v1725, uint16_t const
    *a_threshold_array, size_t a_threshold_num)
{
	size_t i;

	LOGF(verbose)(LOGL, NAME" set_thresholds {");
	assert(16 == a_threshold_num);
	for (i = 0; a_threshold_num > i; ++i) {
		uint32_t u32;

		u32 = a_threshold_array[i];
		LOGF(verbose)(LOGL, " [%"PRIz"]=0x%08x.", i, u32);
		MAP_WRITE(a_v1725->sicy_map, channel_n_trigger_threshold(i),
		    u32);
	}
	LOGF(verbose)(LOGL, NAME" set_thresholds }");
}
