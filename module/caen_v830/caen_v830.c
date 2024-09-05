/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
 * Bastian Löher
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

#include <module/caen_v830/caen_v830.h>
#include <sched.h>
#include <module/caen_v830/internal.h>
#include <module/caen_v830/offsets.h>
#include <module/map/map.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/time.h>

#define NAME "Caen v830"
#define DMA_FILLER 0x08300830
#define NO_DATA_TIMEOUT 1.0

enum {
	CONTROL_TRIGGER_RANDOM   = 0x01,
	CONTROL_TRIGGER_PERIODIC = 0x02,
	CONTROL_DATA_26_BITS     = 0x04,
	CONTROL_TEST_MODE_ON     = 0x08,
	CONTROL_BERR_ENABLE      = 0x10,
	CONTROL_HEADER_ENABLE    = 0x20,
	CONTROL_FRONT_CLEAR_MEB  = 0x40,
	CONTROL_AUTO_RESET       = 0x80
};

MODULE_PROTOTYPES(caen_v830);

uint32_t
caen_v830_check_empty(struct Module *a_module)
{
	struct CaenV830Module *v830;
	uint32_t result;
	uint16_t status;

	LOGF(spam)(LOGL, NAME" check_empty {");
	MODULE_CAST(KW_CAEN_V830, v830, a_module);
	status = MAP_READ(v830->v8n0.sicy_map, status);
	result = 0 == (status & 0x1) ? 0 : CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	if (0 != result) {
		log_error(LOGL, "Non-empty V830, meb=0x%08x.",
		    MAP_READ(v830->v8n0.sicy_map, meb));
	}
	LOGF(spam)(LOGL, NAME" check_empty(status=0x%08x) }", status);
	return result;
}

struct Module *
caen_v830_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV830Module *v830;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(v830);
	/* 32 kwords from p. 7, 33 words/event from p. 22. */
	v830->v8n0.module.event_max = 0x8000 / 33;
	caen_v8n0_create(a_crate, a_block, &v830->v8n0, KW_CAEN_V830);
	LOGF(verbose)(LOGL, NAME" create }");
	return (void *)v830;
}

void
caen_v830_deinit(struct Module *a_module)
{
	struct CaenV830Module *v830;

	MODULE_CAST(KW_CAEN_V830, v830, a_module);
	caen_v8n0_deinit(&v830->v8n0);
}

void
caen_v830_destroy(struct Module *a_module)
{
	(void)a_module;
}

struct Map *
caen_v830_get_map(struct Module *a_module)
{
	struct CaenV830Module *v830;

	MODULE_CAST(KW_CAEN_V830, v830, a_module);
	return caen_v8n0_get_map(&v830->v8n0);
}

void
caen_v830_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(27, 31),
		BITS_MASK(26, 26),
		1 << 26)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature }");
}

int
caen_v830_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV830Module *v830;
	unsigned resolution;
	uint16_t control;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_CAEN_V830, v830, a_module);

	v830->channel_mask = config_get_bitmask(a_module->config,
	    KW_CHANNEL_ENABLE, 0, 31);
	MAP_WRITE(v830->v8n0.sicy_map, channel_enable, v830->channel_mask);

	v830->resolution = 26;
	resolution = config_get_int32(a_module->config, KW_RESOLUTION,
	    CONFIG_UNIT_NONE, 26, 32);
	if (32 == resolution) {
		v830->resolution = resolution;
	}
	LOGF(verbose)(LOGL, "Resolution=%u.", v830->resolution);
	control =
	    (CONTROL_TRIGGER_RANDOM) |
	    (!CONTROL_TRIGGER_PERIODIC) |
	    (CONTROL_DATA_26_BITS) |
	    (!CONTROL_TEST_MODE_ON) |
	    (!CONTROL_BERR_ENABLE) |
	    (CONTROL_HEADER_ENABLE) |
	    (!CONTROL_FRONT_CLEAR_MEB) |
	    (!CONTROL_AUTO_RESET);
	if (32 == v830->resolution) {
		control &= ~CONTROL_DATA_26_BITS;
	}
	if (KW_NOBLT != v830->v8n0.blt_mode) {
		control |= CONTROL_BERR_ENABLE;
	}

	MAP_WRITE(v830->v8n0.sicy_map, control, control);

	v830->counter_prev = a_module->event_counter.value =
	    MAP_READ(v830->v8n0.sicy_map, trigger_counter);

	SERIALIZE_IO;

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v830_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV830Module *v830;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V830, v830, a_module);
	caen_v8n0_init_slow(&v830->v8n0);
	return 1;
}

void
caen_v830_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v830_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct CaenV830Module *v830;
	uint32_t const *p, *end;
	uint32_t counter, result;

	(void)a_crate;
	(void)a_do_pedestals;

	LOGF(spam)(LOGL, NAME" parse_data(%p,%"PRIz") {", a_event_buffer->ptr,
	    a_event_buffer->bytes);

	MODULE_CAST(KW_CAEN_V830, v830, a_module);

	result = 0;
	if (0 != a_event_buffer->bytes % sizeof(uint32_t)) {
		log_error(LOGL, "Data size not 32-bit aligned.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto caen_v830_parse_data_end;
	}
	p = a_event_buffer->ptr;
	end = p + a_event_buffer->bytes / sizeof(uint32_t);
	while (end != p && DMA_FILLER == *p) {
		++p;
	}
	counter = v830->counter_prev;
	for (;; ++counter) {
		uint32_t header, trigger_number;
		unsigned count;

		if (end == p) {
			break;
		}
		header = *p;
		if (0x04000000 != (0x04030000 & header) &&
		    v830->v8n0.geo != (int)((0xf8000000 & header) >> 27)) {
			module_parse_error(LOGL, a_event_buffer, p,
			    "Header corrupt");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto caen_v830_parse_data_end;
		}
		trigger_number = 0xffff & header;
		if ((0xffff & counter) != trigger_number) {
			module_parse_error(LOGL, a_event_buffer, p,
			    "Event counter=0x%08x mismatch", counter);
			result = CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH;
			goto caen_v830_parse_data_end;
		}
		count = (0x00fc0000 & header) >> 18;
		++p;
		if (26 == v830->resolution) {
			uint32_t channel_mask;
			unsigned i;

			channel_mask = 0;
			for (i = 0; count > i; ++i) {
				uint32_t word, channel, channel_bit;

				word = *p;
				if (0 != (0x04000000 & word)) {
					module_parse_error(LOGL,
					    a_event_buffer, p,
					    "Data corrupt");
					result =
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v830_parse_data_end;
				}
				channel = (0xf8000000 & word) >> 27;
				channel_bit = 1 << channel;
				if (0 != (channel_mask & channel_bit)) {
					module_parse_error(LOGL,
					    a_event_buffer, p,
					    "Channel repeated");
					result =
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v830_parse_data_end;
				}
				channel_mask |= channel_bit;
				++p;
			}
		} else {
			p += count;
		}
		if (end < p) {
			log_error(LOGL, "Unexpected end of data.");
			result = CRATE_READOUT_FAIL_DATA_MISSING;
			goto caen_v830_parse_data_end;
		}
	}

caen_v830_parse_data_end:
	v830->counter_prev = v830->v8n0.module.event_counter.value;
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}

uint32_t
caen_v830_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	double t_0;
	struct CaenV830Module *v830;
	uint32_t *outp;
	uint32_t result = 0;
	uint32_t event_diff;
	uint32_t meb_events;
	unsigned no_data_wait;
	uint16_t status;

	LOGF(spam)(LOGL, NAME" readout {");

	MODULE_CAST(KW_CAEN_V830, v830, a_module);
	outp = a_event_buffer->ptr;

	/*
	 * We checked the event counter in _dt, let's just make sure there is
	 * enough data.
	 */
	event_diff = COUNTER_DIFF_RAW(*a_module->crate_counter,
	   a_module->crate_counter_prev);
	meb_events = MAP_READ(v830->v8n0.sicy_map, meb_event_number);
	if (event_diff > meb_events) {
		log_error(LOGL, "Expected %u MEB events, have %u.",
		    event_diff,
		    MAP_READ(v830->v8n0.sicy_map, meb_event_number));
		result |= CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH;
	}

	t_0 = -1;
	for (no_data_wait = 0;; ++no_data_wait) {
		double t;

		status = MAP_READ(v830->v8n0.sicy_map, status);
		if (0x0 != (0x1 & status)) {
			break;
		}
		t = time_getd();
		if (0 > t_0) {
			t_0 = t;
		} else if (NO_DATA_TIMEOUT < t - t_0) {
			log_error(LOGL, "Data timeout (%gs), status=%08x.",
			    NO_DATA_TIMEOUT, status);
			result |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto caen_v830_readout_done;
		}
		sched_yield();
	}
	if (0 < no_data_wait) {
		crate_acvt_grow(a_crate);
	}

	if (KW_NOBLT == v830->v8n0.blt_mode) {
		unsigned event;

		for (event = 0; event_diff > event; ++event) {
			uint32_t header;
			unsigned word_count, i;

			if (!MEMORY_CHECK(*a_event_buffer, outp)) {
				result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
				goto caen_v830_readout_done;
			}
			header = MAP_READ(v830->v8n0.sicy_map, meb);
			*outp++ = header;
			word_count = (0x3f & (header >> 18));
			LOGF(spam)(LOGL, "Event %d, reading %d words.",
			    event, word_count);
			if (!MEMORY_CHECK(*a_event_buffer, &outp[word_count -
			    1])) {
				result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
				goto caen_v830_readout_done;
			}
			for (i = 0; word_count > i; ++i) {
				*outp++ = MAP_READ(v830->v8n0.sicy_map, meb);
			}
		}
	} else {
		uintptr_t bytes;
		int ret;

		bytes = event_diff * (1 + bits_get_count(v830->channel_mask)) * sizeof(uint32_t);
		outp = map_align(outp, &bytes, v830->v8n0.blt_mode,
		    DMA_FILLER);
		if (!MEMORY_CHECK(*a_event_buffer, &outp[bytes / sizeof *outp
		    - 1])) {
			result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto caen_v830_readout_done;
		}
		ret = map_blt_read_berr(v830->v8n0.dma_map, 0, outp, bytes);
		/* TODO: Care about broken return val. */
		if (-1 == ret) {
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto caen_v830_readout_done;
		}
		outp += bytes / sizeof *outp;
	}

caen_v830_readout_done:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

uint32_t
caen_v830_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
        struct CaenV830Module *v830;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_CAEN_V830, v830, a_module);
	a_module->event_counter.value =
	    MAP_READ(v830->v8n0.sicy_map, trigger_counter);
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x) }",
	    a_module->event_counter.value);
	return 0;
}

void
caen_v830_setup_(void)
{
	MODULE_SETUP(caen_v830, 0);
}
