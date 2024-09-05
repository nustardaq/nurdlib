/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Bastian Löher
 * Håkan T Johansson
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

#include <module/caen_v1n90/internal.h>
#include <math.h>
#include <sched.h>
#include <stddef.h>
#include <module/caen_v1n90/micro.h>
#include <module/caen_v1n90/offsets.h>
#include <module/map/map.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/time.h>

#define NAME "Caen v1n90"
#define DMA_FILLER 0x1f901f90
#define SEARCH_MARGIN 0x08 /*200ns*/
#define REJECT_MARGIN 0x04 /*100ns*/
#define TYPE_MASK 0xf8000000

enum {
	CTRL_BERREN                            = 0x0001,
	CTRL_TERM                              = 0x0002,
	CTRL_TERM_SW                           = 0x0004,
	CTRL_EMPTY_EVENT_WRITE                 = 0x0008,
	CTRL_ALIGN64                           = 0x0010,
	CTRL_COMPENSATION_ENABLE               = 0x0020,
	CTRL_TEST_FIFO_ENABLE                  = 0x0040,
	CTRL_READ_COMPENSATION_SRAM_ENABLE     = 0x0080,
	CTRL_EVENT_FIFO_ENABLE                 = 0x0100,
	CTRL_EXTENDED_TRIGGER_TIME_TAG_ENABLE  = 0x0200,
	CTRL_16MB_ADDR_RANGE_MEB_ACCESS_ENABLE = 0x1000
};

uint32_t
caen_v1n90_check_empty(struct CaenV1n90Module *a_v1n90)
{
	uint32_t status, result;

	LOGF(spam)(LOGL, NAME" check_empty {");
	status = MAP_READ(a_v1n90->sicy_map, event_fifo_status);
	result = 0 == (0x1 & status) ? 0 : CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	LOGF(spam)(LOGL, NAME" check_empty(0x%08x) }", result);
	return result;
}

void
caen_v1n90_create(struct ConfigBlock const *a_block, struct CaenV1n90Module
    *a_v1n90)
{
	LOGF(verbose)(LOGL, NAME" create {");

	/* 1024 from manual p. 43. */
	a_v1n90->module.event_max = 1024;

	a_v1n90->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", a_v1n90->address);

	a_v1n90->dma_map = NULL;

	a_v1n90->module.event_counter.mask = 0;

	LOGF(verbose)(LOGL, NAME" create }");
}

void
caen_v1n90_deinit(struct CaenV1n90Module *a_v1n90)
{
	LOGF(info)(LOGL, NAME" deinit {");
	map_unmap(&a_v1n90->sicy_map);
	map_unmap(&a_v1n90->dma_map);
	LOGF(info)(LOGL, NAME" deinit }");
}

void
caen_v1n90_destroy(struct CaenV1n90Module *a_v1n90)
{
	(void)a_v1n90;
}

struct Map *
caen_v1n90_get_map(struct CaenV1n90Module *a_v1n90)
{
	return a_v1n90->sicy_map;
}

void
caen_v1n90_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(0, 4),
		BITS_MASK(27, 31),
		8 << 23)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature }");
}

void
caen_v1n90_init_fast(struct CaenV1n90Module *a_v1n90, enum Keyword a_subtype)
{
	struct ModuleGate gate;
	enum Keyword const c_edges[] = {
		KW_PAIR,
		KW_TRAILING,
		KW_LEADING,
		KW_TRAILING_AND_LEADING
	};
	enum Keyword edge;
	int deadtime;

	LOGF(info)(LOGL, NAME" init_fast {");

	module_gate_get(&gate, a_v1n90->module.config,
	    -(0x800 * 25.), 0x28 * 25.,
	    1 * 25., 0xfff * 25.);
	a_v1n90->gate_offset = (int16_t)floor(gate.time_after_trigger_ns /
	    25.0);
	a_v1n90->gate_width = (int16_t)ceil(gate.width_ns / 25.0);
	LOGF(verbose)(LOGL,
	    "Trigger window=(ofs=%.1fns=0x%04x,width=%.1fns=0x%04x).",
	    gate.time_after_trigger_ns, a_v1n90->gate_offset,
	    gate.width_ns, a_v1n90->gate_width);

	edge = CONFIG_GET_KEYWORD(a_v1n90->module.config, KW_EDGE, c_edges);
	a_v1n90->edge_code = 0;
	if (KW_PAIR == edge) {
		a_v1n90->edge_code |= 0x0;
	} else if (KW_TRAILING == edge) {
		a_v1n90->edge_code |= 0x1;
	} else if (KW_LEADING == edge) {
		a_v1n90->edge_code |= 0x2;
	} else if (KW_TRAILING_AND_LEADING == edge) {
		a_v1n90->edge_code |= 0x3;
	}
	LOGF(verbose)(LOGL, "Edge microcode = 0x%04x.", a_v1n90->edge_code);

	/*
	 * NOTE: Leading/trailing options use the single-value 24xx, and pair
	 * the double-value 25xx.
	 */
	if (0x2200 == a_v1n90->edge_code) {
		int const c_25xx_code_lsb[] = {100, 200, 400, 800, 1600, 3200,
			6250, 12500};
		int const c_25xx_code_msb[] = {100, 200, 400, 800, 1600, 3200,
			6250, 12500, 25000, 50000, 100000, 200000, 400000,
			800000};
		int resolution[2];
		size_t i;

		CONFIG_GET_INT_ARRAY(resolution, a_v1n90->module.config,
		    KW_RESOLUTION, CONFIG_UNIT_PS, 0, 1e6);
		for (i = 0; LENGTH(c_25xx_code_lsb) > i; ++i) {
			if (c_25xx_code_lsb[i] == resolution[0]) {
				a_v1n90->resolution_code = i;
				break;
			}
		}
		if (LENGTH(c_25xx_code_lsb) == i) {
			log_die(LOGL, "Invalid leading edge resolution %d ps"
			    " for pair mode, read the manual.",
			    resolution[0]);
		}
		for (i = 0; LENGTH(c_25xx_code_msb) > i; ++i) {
			if (c_25xx_code_msb[i] == resolution[1]) {
				a_v1n90->resolution_code |= i << 8;
				break;
			}
		}
		if (LENGTH(c_25xx_code_msb) == i) {
			log_die(LOGL, "Invalid width resolution %d ps for "
			    "pair mode, read the manual.", resolution[1]);
		}
	} else {
		int resolution;

		resolution = config_get_int32(a_v1n90->module.config,
		    KW_RESOLUTION, CONFIG_UNIT_PS, 0, 900);
		a_v1n90->resolution_code = 0;
		if (800 == resolution) {
			a_v1n90->resolution_code |= 0x0;
		} else if (200 == resolution) {
			a_v1n90->resolution_code |= 0x1;
		} else if (100 == resolution) {
			a_v1n90->resolution_code |= 0x2;
		} else if (25 == resolution) {
			a_v1n90->resolution_code |= 0x3;
		} else {
			log_die(LOGL, "Invalid LSB/resolution %d ps, should "
			    "be 25, 100, 200, or 800 ps.", resolution);
		}

		if (KW_CAEN_V1190 == a_subtype && 25 == resolution) {
			log_die(LOGL, "CAEN 1190 does not support "
				"25ps resolution!");
		}
	}

	deadtime = config_get_int32(a_v1n90->module.config, KW_DEADTIME,
	    CONFIG_UNIT_NS, 0, 200);
	a_v1n90->deadtime_code = 0;
	if (5 == deadtime) {
		a_v1n90->deadtime_code |= 0x0;
	} else if (10 == deadtime) {
		a_v1n90->deadtime_code |= 0x1;
	} else if (30 == deadtime) {
		a_v1n90->deadtime_code |= 0x2;
	} else if (100 == deadtime) {
		a_v1n90->deadtime_code |= 0x3;
	} else {
		log_die(LOGL, "Invalid eadtime %d ns, should be 5, 10, 30, "
		    "or 100 ns.", deadtime);
	}

	LOGF(info)(LOGL, NAME" init_fast }");
}

void
caen_v1n90_init_slow(struct CaenV1n90Module *a_v1n90)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_MBLT,
		KW_BLT_2EVME,
		KW_BLT_2ESST,
		KW_FF,
		KW_NOBLT
	};
	uint16_t revision, rom_vers, control;

	LOGF(info)(LOGL, NAME" init_slow {");

	a_v1n90->sicy_map = map_map(a_v1n90->address, MAP_SIZE,
	    KW_NOBLT, 0, 0, MAP_POKE_REG(testreg), MAP_POKE_REG(testreg), 0);

	MAP_WRITE(a_v1n90->sicy_map, module_reset, 0);

	/* Initialization can take some time, sleep sometimes needed... */
	time_sleep(10e-3);
	SERIALIZE_IO;

	revision = MAP_READ(a_v1n90->sicy_map, firmware_revision);
	LOGF(info)(LOGL, "Firmware revision = %d.%d (%04x).",
	    (0xf0 & revision) >> 4, 0x0f & revision, revision);

	rom_vers = MAP_READ(a_v1n90->sicy_map, rom_vers);
	LOGF(info)(LOGL, "ROM version = %04x.", rom_vers);

	control = !CTRL_BERREN |
	    !CTRL_TERM |
	    !CTRL_TERM_SW |
	    CTRL_EMPTY_EVENT_WRITE |
	    CTRL_ALIGN64 |
	    CTRL_COMPENSATION_ENABLE |
		CTRL_EVENT_FIFO_ENABLE |
	    !CTRL_TEST_FIFO_ENABLE |
	    !CTRL_READ_COMPENSATION_SRAM_ENABLE |
	    !CTRL_EXTENDED_TRIGGER_TIME_TAG_ENABLE |
	    !CTRL_16MB_ADDR_RANGE_MEB_ACCESS_ENABLE;

	MAP_WRITE(a_v1n90->sicy_map, control, control);
	MAP_WRITE(a_v1n90->sicy_map, geo_address, a_v1n90->module.id);

	a_v1n90->module.event_counter.value = MAP_READ(a_v1n90->sicy_map,
	    event_counter);
	LOGF(verbose)(LOGL, "Event counter=0x%08x",
	    a_v1n90->module.event_counter.value);
	a_v1n90->header_counter = 0;

	a_v1n90->blt_mode = CONFIG_GET_KEYWORD(a_v1n90->module.config,
	    KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "BLT mode = %s.",
	    keyword_get_string(a_v1n90->blt_mode));

	if (KW_NOBLT != a_v1n90->blt_mode) {
		a_v1n90->dma_map = map_map(a_v1n90->address, 0x1000,
		    a_v1n90->blt_mode, 0, 0,
		    MAP_POKE_REG(testreg), MAP_POKE_REG(testreg), 0);
	}

	a_v1n90->parse.expect = EXPECT_DMA_HEADER;

	LOGF(info)(LOGL, NAME" init_slow }");
}

int
caen_v1n90_micro_init_fast(struct ModuleList const *a_list)
{
	struct Module *module;
	struct CaenV1n90Module *v1n90;
	size_t num;
	int skip;

	(void)v1n90;
	num = 0;
	TAILQ_FOREACH(module, a_list, next) {
		if (KW_CAEN_V1190 == module->type ||
		    KW_CAEN_V1290 == module->type) {
			v1n90 = (void *)module;
			++num;
		}
	}
	if (0 == num) {
		return 1;
	}

	LOGF(info)(LOGL, NAME" micro_init_fast (patience...) {");
	skip = 0;

#define MICRO_WRITE(reg) do {\
		micro_write(a_list, reg, &skip);\
		SERIALIZE_IO;\
	} while(0);

#define MICRO_WRITE_MEMBER(member) do {\
		micro_write_member(a_list, OFFSETOF(*v1n90, member),\
			&skip);\
		SERIALIZE_IO;\
	} while(0);

	LOGF(info)(LOGL, NAME" micro_init_fast }");
	return !skip;
}

int
caen_v1n90_micro_init_slow(struct ModuleList const *a_list)
{
	struct Module *module;
	struct CaenV1n90Module *v1n90;
	size_t num;
	int skip;

	v1n90 = NULL;
	num = 0;
	TAILQ_FOREACH(module, a_list, next) {
		if (KW_CAEN_V1190 == module->type ||
		    KW_CAEN_V1290 == module->type) {
			v1n90 = (void *)module;
			++num;
		}
	}
	if (0 == num) {
		return 1;
	}

	LOGF(info)(LOGL, NAME" micro_init_slow (patience...) {");
	skip = 0;

	/* Trigger matching mode. */
	MICRO_WRITE(MICRO_TRIG_MATCH);

	/* Window. */
	MICRO_WRITE(MICRO_SET_WIN_WIDTH);
	MICRO_WRITE_MEMBER(gate_width);

	MICRO_WRITE(MICRO_SET_WIN_OFFSET);
	MICRO_WRITE_MEMBER(gate_offset);

	/* Search margin in cycles. */
	MICRO_WRITE(MICRO_SET_SW_MARGIN);
	MICRO_WRITE(SEARCH_MARGIN);

	/* Reject margin in cycles. */
	MICRO_WRITE(MICRO_SET_REJ_MARGIN);
	MICRO_WRITE(REJECT_MARGIN);

	/* Subtraction, TDC times against match window start. */
	MICRO_WRITE(MICRO_EN_SUB_TRG);

	/* Detection mode. */
	MICRO_WRITE(MICRO_SET_DETECTION);
	MICRO_WRITE_MEMBER(edge_code);

	/* Timing resolution. */
	MICRO_WRITE(MICRO_SET_TR_LEAD_LSB);
	MICRO_WRITE_MEMBER(resolution_code);

	/* Deadtime between hits. */
	MICRO_WRITE(MICRO_SET_DEAD_TIME);
	MICRO_WRITE_MEMBER(deadtime_code);

	/* Set header+trailer. */
	/* TODO: Config? */
	MICRO_WRITE(MICRO_EN_HEAD_TRAIL);

	/* Hits limit. */
	MICRO_WRITE(MICRO_SET_EVENT_SIZE);
	MICRO_WRITE(MICRO_EVENT_SIZE_INF);

	/* Enable TDC error. */
	MICRO_WRITE(MICRO_EN_ERROR_MARK);

	/* Enable TDC bypass on error. */
	MICRO_WRITE(MICRO_EN_ERROR_BYPASS);

	/* FIFO size. */
	MICRO_WRITE(MICRO_SET_FIFO_SIZE);
	MICRO_WRITE(MICRO_FIFO_SIZE_256);

	/* Enable all channels. */
	MICRO_WRITE(MICRO_EN_ALL_CH);

	/*
	 * Check everything !
	 */

	/* Check acquisition mode. */
	MICRO_WRITE(MICRO_READ_ACQ_MOD);
	SERIALIZE_IO;
	TAILQ_FOREACH(module, a_list, next) {
		int mode;

		if (KW_CAEN_V1190 != module->type &&
		    KW_CAEN_V1290 != module->type) {
			continue;
		}
		mode = micro_read((void *)module, &skip) & 0x1;
		if (skip) {
			return 0;
		}
		if (MICRO_ACQ_MODE_TRIG != mode) {
			log_die(LOGL, NAME" runs in continuous mode (%d)",
			    mode);
			return 0;
		}
	}
	SERIALIZE_IO;

	/* FIFO size. */
	MICRO_WRITE(MICRO_READ_FIFO_SIZE);
	TAILQ_FOREACH(module, a_list, next) {
		uint16_t fifo_size;

		if (KW_CAEN_V1190 != module->type &&
		    KW_CAEN_V1290 != module->type) {
			continue;
		}
		fifo_size = micro_read((void *)module, &skip);
		if (skip) {
			return 0;
		}
		fifo_size = 2 << fifo_size;
		if (256 != fifo_size) {
			log_die(LOGL, NAME" has wrong FIFO size %u "
				" instead of 256!", fifo_size);
		} else {
			LOGF(verbose)(LOGL, NAME" %d: FIFO size=%d.",
			    module->id, fifo_size);
		}
	}
	SERIALIZE_IO;

	/* Hits limit. */
	MICRO_WRITE(MICRO_READ_EVENT_SIZE);
	TAILQ_FOREACH(module, a_list, next) {
		int hits_per_event;

		if (KW_CAEN_V1190 != module->type &&
		    KW_CAEN_V1290 != module->type) {
			continue;
		}
		hits_per_event = micro_read((void *)module, &skip);
		if (skip) {
			return 0;
		}
		if (MICRO_EVENT_SIZE_128 >= hits_per_event) {
			log_die(LOGL, NAME"%d: Max hits/event=%d.", 1 <<
			    module->id, hits_per_event);
		} else if (MICRO_EVENT_SIZE_INF == hits_per_event) {
			LOGF(verbose)(LOGL,
			    NAME" %d: Max hits/event=unlimited.",
			    module->id);
		} else {
			log_die(LOGL, NAME" has meaningless number of hits "
			    "per event!");
			return 0;
		}
	}

	/* Header + footers. */
	MICRO_WRITE(MICRO_READ_HEAD_TRAIL);
	TAILQ_FOREACH(module, a_list, next) {
		int wants_header_footer;

		if (KW_CAEN_V1190 != module->type &&
		    KW_CAEN_V1290 != module->type) {
			continue;
		}
		wants_header_footer = micro_read((void *)module, &skip);
		if (skip) {
			return 0;
		}
		(void)wants_header_footer;
/*		if (0 != wants_header_footer) {
			log_die(LOGL, NAME" Wants to write "
			    "headers+footers!");
			return 0;
		}*/
	}

#define CHECK_CONFIG(reg, entry, text) do {\
	micro_write(a_list, reg, &skip);\
	SERIALIZE_IO;\
	TAILQ_FOREACH(module, a_list, next) {\
		int val;\
		if (KW_CAEN_V1190 != module->type &&\
		    KW_CAEN_V1290 != module->type) {\
			continue;\
		}\
		val = micro_read((void *)module, &skip);\
		if (skip) {\
			return 0;\
		}\
		if ((v1n90->entry & 0xFF) != val) {\
			log_die(LOGL, NAME" Has wrong "text" 0x%02x instead "\
				" of 0x%02x", val, v1n90->entry);\
			return 0;\
		}\
	}\
	SERIALIZE_IO;\
} while (0);

/*	CHECK_CONFIG(MICRO_READ_DEAD_TIME, deadtime_code, "dead time code");
	CHECK_CONFIG(MICRO_READ_RES, resolution_code, "resolution code");
	CHECK_CONFIG(MICRO_READ_DETECTION, edge_code, "edge code");*/

/*MM: This check quite often gives timeouts...*/
/* TODO: This must be investigated! */
#if 0
	MICRO_WRITE(MICRO_READ_TRG_CONF);
	TAILQ_FOREACH(module, a_list, next) {
		int width, offset, extra, reject, subtract;

		if (KW_CAEN_V1190 != module->type &&
		    KW_CAEN_V1290 != module->type) {
			continue;
		}
		width = micro_read((void *)module, &skip);
		offset = micro_read((void *)module, &skip);
		extra = micro_read((void *)module, &skip);
		reject = micro_read((void *)module, &skip);
		subtract = micro_read((void *)module, &skip);
		if (skip) {
			return 0;
		}
		if (v1n90->gate_width != width) {
			log_die(LOGL, NAME" has wrong gate width 0x%04x", width);
			return 0;
		}
		if (v1n90->gate_offset != offset) {
			log_die(LOGL, NAME" has wrong offseet 0x%04x", offset);
			return 0;
		}
		if (SEARCH_MARGIN != extra) {
			log_die(LOGL, NAME" has wrong search margin 0x%04x", extra);
			return 0;
		}
		if (REJECT_MARGIN != reject) {
			log_die(LOGL, NAME" has wrong reject margin 0x%04x", reject);
			return 0;
		}
		if (1 != subtract) {
			log_die(LOGL, NAME" Does not subtract trigger time");
			return 0;
		}
	}
	SERIALIZE_IO;
#endif

	LOGF(info)(LOGL, NAME" micro_init_slow }");
	return !skip;
}

uint32_t
caen_v1n90_parse_data(struct CaenV1n90Module *a_v1n90, struct EventConstBuffer
    const *a_event_buffer, int a_do_pedestals)
{
	uint32_t const *p32;
	uint32_t const *end;
	uint32_t result;
	/* TODO: Shouldn't this be persistent, i.e. in v1n90.parse.*? */
	uint32_t tdc_word_cnt = 0;

	(void)a_do_pedestals;
	(void)tdc_word_cnt;

	LOGF(spam)(LOGL, NAME" parse_data {");

	result = 0;
	if (0 != a_event_buffer->bytes % sizeof(uint32_t)) {
		log_error(LOGL, "Event buffer size not 32-bit aligned.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto caen_v1n90_parse_data_done;
	}
	p32 = a_event_buffer->ptr;
	end = p32 + a_event_buffer->bytes / sizeof(uint32_t);
	for (; p32 < end; ++p32) {
		uint32_t u32;

		u32 = *p32;
		switch (a_v1n90->parse.expect) {
		case EXPECT_DMA_HEADER:
			if (DMA_FILLER == u32) {
			} else if (0x40000000 == (TYPE_MASK & u32)) {
				/*
				uint32_t event_count;
				event_count = (u32 & BITS_MASK(5, 26)) >> 5;
				(void)event_count;
				*/
				/* TODO: Check this counter + GEO? */
				/* TODO: If disabled, go to payload. */
				a_v1n90->parse.expect = EXPECT_TDC_HEADER;
			} else {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Global header corrupt.");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto caen_v1n90_parse_data_done;
			}
			break;
		case EXPECT_TDC_HEADER:
			if (0x08000000 == (TYPE_MASK & u32)) {
				/*
				uint32_t tdc_num;
				tdc_num = (u32 & BITS_MASK(24, 25)) >> 24;
				printf("tdc number = %u\n", tdc_num);
				*/
				/* TODO: Check Event-ID and Bunch-ID? */
				a_v1n90->parse.expect = EXPECT_TDC_PAYLOAD;
				++tdc_word_cnt;
			} else {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "TDC header corrupt.");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto caen_v1n90_parse_data_done;
			}
			break;
		case EXPECT_TDC_PAYLOAD:
			++tdc_word_cnt;
			if (0x00000000 == (TYPE_MASK & u32)) {
			} else if (0x20000000 == (TYPE_MASK & u32)) {
				/* TODO: This is optional. */
				log_error(LOGL,
				    NAME" TDC [%1x] reports error 0x%4x.",
				    ((u32 >> 24) & 0x3), (u32 & 0x7FFF));
			} else if (0x18000000 == (TYPE_MASK & u32)) {
				/*
				uint32_t word_count;
				word_count = (u32 & BITS_MASK(0, 11));
				printf("word count = %u, word counter = %u\n",
				    word_count, tdc_word_cnt);
				if (word_count != tdc_word_cnt) {
					module_parse_error(LOGL,
					    a_event_buffer, p32,
					    "TDC trailer word count does not "
					    "match actual number of words.");
					result =
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v1n90_parse_data_done;
				}
				*/
				a_v1n90->parse.expect =
				    EXPECT_TIMETAG_OR_TRAILER_OR_TDC_HEADER;
				tdc_word_cnt = 0;
			} else {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "TDC payload corrupt.");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto caen_v1n90_parse_data_done;
			}
			break;
		case EXPECT_TIMETAG_OR_TRAILER_OR_TDC_HEADER:
			if (0x88000000 == (TYPE_MASK & u32)) {
				a_v1n90->parse.expect = EXPECT_TRAILER;
			} else if (0x80000000 == (TYPE_MASK & u32)) {
				/* TODO: Check word-count and geo! */
				a_v1n90->parse.expect = EXPECT_DMA_HEADER;
			} else if (0x08000000 == (TYPE_MASK & u32)) {
				/* TODO: Check Event-ID and Bunch-ID? */
				/*
				uint32_t tdc_num;
				tdc_num = (u32 & BITS_MASK(24, 25)) >> 24;
				printf("tdc number = %u \n", tdc_num);
				*/
				a_v1n90->parse.expect = EXPECT_TDC_PAYLOAD;
				++tdc_word_cnt;
			} else {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Trailer or next TDC event corrupt.");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto caen_v1n90_parse_data_done;
			}
			break;
		case EXPECT_TRAILER:
			if (0x80000000 == (TYPE_MASK & u32)) {
				/* TODO: Check word-count and geo! */
				a_v1n90->parse.expect = EXPECT_DMA_HEADER;
			} else {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Trailer corrupt.");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto caen_v1n90_parse_data_done;
			}
			break;
		}
	}

caen_v1n90_parse_data_done:
	LOGF(spam)(LOGL, NAME" parse_data(0x%08x) }", result);
	return result;
}

uint32_t
caen_v1n90_readout(struct Crate *a_crate, struct CaenV1n90Module *a_v1n90,
    struct EventBuffer *a_event_buffer)
{
	uint32_t *outp, *header;
	uint32_t event_count, result;
	unsigned event_diff, event_stored, i, fifo_stored, word_count;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");

	header = outp = a_event_buffer->ptr;
	result = 0;

	if (a_v1n90->was_full) {
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	}

	event_diff = COUNTER_DIFF_RAW(*a_v1n90->module.crate_counter,
	   a_v1n90->module.crate_counter_prev);
	event_stored = MAP_READ(a_v1n90->sicy_map, event_stored);
	if (event_diff > event_stored) {
		log_error(LOGL, "Events stored=%u, but event_diff=%u.",
		    event_stored, event_diff);
		result |= CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH;
	}
	fifo_stored = MAP_READ(a_v1n90->sicy_map, event_fifo_stored);
	if (event_diff != fifo_stored) {
		log_error(LOGL, "FIFO stored=%u, but event_diff=%u.",
		    fifo_stored, event_diff);
		result |= CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH;
	}

	/* Figure out how much to move. */
	event_count = 0;
	word_count = 0;
	for (i = 0; fifo_stored > i; ++i) {
		uint32_t counts;

		counts = MAP_READ(a_v1n90->sicy_map, event_fifo);
		event_count += (0xffff0000 & counts) >> 16;
		word_count += 0x0000ffff & counts;
	}
	LOGF(spam)(LOGL, "Event-count=%d, word-count=%d.", event_count,
	    word_count);
	if (word_count * sizeof(uint32_t) > a_event_buffer->bytes) {
		log_error(LOGL, "FIFO too big for event destination.");
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto caen_v1n90_readout_done;
	}
	if (KW_NOBLT == a_v1n90->blt_mode) {
		/* Single cycle moving. */
		for (i = 0; word_count > i; ++i) {
			*outp++ = MAP_READ(a_v1n90->sicy_map, output_buffer);
		}
	} else {
		/* BLT moving. */
		uintptr_t bytes;
		int ret;

		bytes = word_count * sizeof(uint32_t);
		outp = map_align(outp, &bytes, a_v1n90->blt_mode, DMA_FILLER);
		ret = map_blt_read(a_v1n90->dma_map, 0, outp, bytes);
		if (-1 == ret) {
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto caen_v1n90_readout_done;
		}
		outp += bytes / 4;
	}
	event_count = (0x07ffffe0 & *header) >> 5;
	if (a_v1n90->header_counter != event_count) {
		log_error(LOGL, "Event counter mismatch "
		    "(header=0x%08x,prev cnt=0x%08x).", *header,
		    a_v1n90->header_counter);
		result |= CRATE_READOUT_FAIL_DATA_CORRUPT;
	}
	a_v1n90->header_counter = 0x003fffff & (a_v1n90->header_counter +
	    event_diff);

caen_v1n90_readout_done:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

uint32_t
caen_v1n90_readout_dt(struct CaenV1n90Module *a_v1n90)
{
	uint32_t result;
	uint16_t status;

	LOGF(spam)(LOGL, NAME" readout_dt {");
	result = 0;
	status = MAP_READ(a_v1n90->sicy_map, status);
	LOGF(spam)(LOGL, "Status = 0x%04x.", status);
	if (0 != (0x3c0 & status)) {
		log_error(LOGL, "TDC error (status=0x%04x).", status);
		result = CRATE_READOUT_FAIL_GENERAL;
		goto caen_v1n90_readout_dt_done;
	}
	if (0 == (0x1 & status)) {
		goto caen_v1n90_readout_dt_done;
	}
	status = MAP_READ(a_v1n90->sicy_map, event_fifo_status);
	LOGF(spam)(LOGL, "FIFO status = 0x%04x.", status);
	if (0 == (0x1 & status)) {
		goto caen_v1n90_readout_dt_done;
	}
	a_v1n90->was_full = 0;
	if (0 != (0x2 & status)) {
		log_error(LOGL, "FIFO was full.");
		a_v1n90->was_full = 1;
	}
	a_v1n90->module.event_counter.value = MAP_READ(a_v1n90->sicy_map,
	    event_counter);
caen_v1n90_readout_dt_done:
	LOGF(spam)(LOGL, NAME" readout_dt(result=0x%08x,ctr=0x%08x) }",
	    result, a_v1n90->module.event_counter.value);
	return result;
}

void
caen_v1n90_register_list_pack(struct CaenV1n90Module *a_v1n90, struct
    PackerList *a_list, unsigned a_ch_word_num)
{
	struct ModuleList module_list;
	struct Packer *packer;
	uint8_t *nump;
	size_t num;
	unsigned i;
	int skip;
	uint16_t u16;

	LOGF(debug)(LOGL, NAME" register_list_pack {");

	TAILQ_INIT(&module_list);
	TAILQ_INSERT_TAIL(&module_list, (struct Module *)&a_v1n90->module,
	    next);

	skip = 0;

#define READ(NAME) do {\
		micro_write(&module_list, MICRO_READ_##NAME, &skip);\
		u16 = micro_read(a_v1n90, &skip);\
		packer = packer_list_get(a_list, 16); \
		pack16(packer, MICRO_READ_##NAME);\
		packer = packer_list_get(a_list, 16); \
		pack16(packer, u16);\
		++num; \
	} while (0)

	packer = packer_list_get(a_list, 8);
	nump = &((uint8_t *)packer->data)[packer->ofs];
	num = 0;
	pack8(packer, 0);

	READ(ACQ_MOD);

	for (i = 0; i < 5; ++i) {
		micro_write(&module_list, MICRO_READ_TRG_CONF, &skip);
		u16 = micro_read(a_v1n90, &skip);
		packer = packer_list_get(a_list, 16);
		pack16(packer, MICRO_READ_TRG_CONF);
		packer = packer_list_get(a_list, 16);
		pack16(packer, u16);
		++num;
	}

	READ(DETECTION);
	READ(RES);
	READ(DEAD_TIME);
	READ(HEAD_TRAIL);
	READ(EVENT_SIZE);
	READ(ERROR_TYPES);
	READ(FIFO_SIZE);

	for (i = 0; i < a_ch_word_num; ++i) {
		micro_write(&module_list, MICRO_READ_EN_CHANNEL, &skip);
		u16 = micro_read(a_v1n90, &skip);
		packer = packer_list_get(a_list, 16);
		pack16(packer, MICRO_READ_EN_CHANNEL);
		packer = packer_list_get(a_list, 16);
		pack16(packer, u16);
		++num;
	}

	READ(FIRMWARE);

	*nump = num;

	LOGF(debug)(LOGL, NAME" register_list_pack }");
}
