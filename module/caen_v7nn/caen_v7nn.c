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

#include <module/caen_v7nn/internal.h>
#include <sched.h>
#include <module/caen_v7nn/offsets.h>
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

#define NAME "Caen v7nn"


#define DMA_FILLER 0x07ff07ff
#define NO_DATA_TIMEOUT 1.0

#define BS1_BERR_FLAG          0x0008
#define BS1_SELECT_ADDRESS     0x0010
#define BS1_SOFTW_RESET        0x0080

#define BS2_TEST_MEM           0x0001
#define BS2_OFFLINE            0x0002
#define BS2_CLEAR_DATA         0x0004
#define BS2_OVER_RANGE_INCL    0x0008
#define BS2_LOW_THRESHOLD_INCL 0x0010
#define BS2_TEST_ACQ           0x0040
#define BS2_SLIDING_SCALE      0x0080
#define BS2_FINE_THRESHOLD     0x0100
#define BS2_AUTO_INCR          0x0800
#define BS2_EMPTY_INCL         0x1000
#define BS2_SLIDE_SUB_SUPPR    0x2000
#define BS2_ALL_TRG            0x4000

#define CT1_BLKEND             0x0004
#define CT1_PROG_RESET         0x0010
#define CT1_BERR_ENABLE        0x0020
#define CT1_ALIGN64            0x0040

#define ST1_AMNESIA            0x0010

#define COUNTER_MASK BITS_MASK_TOP(23)
#define COUNTER_VALUE(c) (COUNTER_MASK & (c))

static uint32_t	event_counter_get(struct CaenV7nnModule const *) FUNC_RETURNS;
static void	threshold_set(struct CaenV7nnModule *, uint16_t const *,
    size_t);

uint32_t
caen_v7nn_check_empty(struct CaenV7nnModule *a_v7nn)
{
	uint32_t result;
	uint16_t status;

	LOGF(spam)(LOGL, NAME" check_empty {");
	status = MAP_READ(a_v7nn->sicy_map, status_2);
	result = 2 == (2 & status) ? 0 : CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	LOGF(spam)(LOGL, NAME" check_empty(check_empty=%08x) }", result);
	return result;
}

void
caen_v7nn_create(struct ConfigBlock *a_block, struct CaenV7nnModule *a_v7nn,
    enum Keyword a_child_type)
{
	LOGF(verbose)(LOGL, NAME" create {");

	a_v7nn->child_type = a_child_type;

	a_v7nn->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", a_v7nn->address);

	a_v7nn->do_auto_pedestals = config_get_boolean(a_block,
	    KW_AUTO_PEDESTALS);
	if (a_v7nn->do_auto_pedestals) {
		LOGF(info)(LOGL, "Auto pedestals enabled.");
	} else {
		LOGF(verbose)(LOGL, "Auto pedestals disabled.");
	}

	a_v7nn->dma_map = NULL;

	a_v7nn->module.event_counter.mask = COUNTER_MASK;

	if (KW_CAEN_V785N == a_v7nn->child_type) {
		a_v7nn->number_of_channels = 16;
	} else {
		a_v7nn->number_of_channels = 32;
	}

	LOGF(verbose)(LOGL, NAME" create }");
}

void
caen_v7nn_deinit(struct CaenV7nnModule *a_v7nn)
{
	LOGF(info)(LOGL, NAME" deinit {");
	map_unmap(&a_v7nn->sicy_map);
	map_unmap(&a_v7nn->dma_map);
	LOGF(info)(LOGL, NAME" deinit }");
}

void
caen_v7nn_destroy(struct CaenV7nnModule *a_v7nn)
{
	(void)a_v7nn;
}

struct Map *
caen_v7nn_get_map(struct CaenV7nnModule *a_v7nn)
{
	return a_v7nn->sicy_map;
}

void
caen_v7nn_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(27, 31),
		BITS_MASK(24, 26) | BITS_MASK(14, 15),
		1 << 25)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature }");
}

void
caen_v7nn_init_fast(struct Crate *a_crate, struct CaenV7nnModule *a_v7nn)
{
	char str[64];
	uint16_t *threshold_array;
	unsigned ofs, i;
	uint16_t bs2;

	LOGF(info)(LOGL, NAME" init_fast {");

	/* Nurdlib default, verbose mode. */
	bs2 =
	    !BS2_TEST_MEM           |
	    !BS2_OFFLINE            |
	     BS2_CLEAR_DATA         |
	     BS2_OVER_RANGE_INCL    |
	    !BS2_LOW_THRESHOLD_INCL |
	    !BS2_TEST_ACQ           |
	     BS2_SLIDING_SCALE      |
	     BS2_FINE_THRESHOLD     |
	     BS2_AUTO_INCR          |
	    !BS2_EMPTY_INCL         |
	    !BS2_SLIDE_SUB_SUPPR    |
	     BS2_ALL_TRG;
	if (!config_get_boolean(a_v7nn->module.config, KW_SLIDING_SCALE)) {
		bs2 &= ~BS2_SLIDING_SCALE;
	}
	if (config_get_boolean(a_v7nn->module.config, KW_SUPPRESS_OVER_RANGE))
	{
		bs2 &= ~BS2_OVER_RANGE_INCL;
	}
	if (crate_acvt_has(a_crate)) {
		bs2 |= BS2_EMPTY_INCL;
	}
	MAP_WRITE(a_v7nn->sicy_map, bit_set_2, bs2);
	MAP_WRITE(a_v7nn->sicy_map, bit_clear_2, ~bs2 | BS2_CLEAR_DATA);
	SERIALIZE_IO;

	a_v7nn->channel_enable = config_get_bitmask(a_v7nn->module.config,
	    KW_CHANNEL_ENABLE, 0, a_v7nn->number_of_channels - 1);

	MALLOC(threshold_array, a_v7nn->number_of_channels);
	config_get_int_array(threshold_array,
	    sizeof *threshold_array * a_v7nn->number_of_channels,
	    sizeof *threshold_array,
	    a_v7nn->module.config, KW_THRESHOLD, CONFIG_UNIT_NONE, 0, 4095);

	LOGF(verbose)(LOGL, "Thresholds {");
	ofs = 0;
	for (i = 0; a_v7nn->number_of_channels > i;) {
		if (0 == ((1 << i) & a_v7nn->channel_enable)) {
			ofs += snprintf_(str + ofs, sizeof str - ofs,
			    " -off-");
		} else {
			ofs += snprintf_(str + ofs, sizeof str - ofs,
			    " 0x%03x", threshold_array[i]);
		}
		++i;
		if (0 == (7 & i)) {
			LOGF(verbose)(LOGL, "%s", str);
			ofs = 0;
		}
	}
	LOGF(verbose)(LOGL, "Thresholds }");

	threshold_set(a_v7nn, threshold_array, a_v7nn->number_of_channels);
	free(threshold_array);

	SERIALIZE_IO;
	bs2 = MAP_READ(a_v7nn->sicy_map, bit_set_2);
	LOGF(verbose)(LOGL, "Bit set 2 = 0x%04x.", bs2);

	LOGF(info)(LOGL, NAME" init_fast }");
}

void
caen_v7nn_init_slow(struct Crate *a_crate, struct CaenV7nnModule *a_v7nn)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_MBLT,
		KW_FF,
		KW_NOBLT
	};
	uint16_t revision, status;

	LOGF(info)(LOGL, NAME" init_slow {");

	a_v7nn->blt_mode = CONFIG_GET_KEYWORD(a_v7nn->module.config,
	    KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "Using BLT mode = %s.",
	    keyword_get_string(a_v7nn->blt_mode));

	a_v7nn->do_berr = config_get_boolean(a_v7nn->module.config, KW_BERR);
	LOGF(verbose)(LOGL, "Do BERR BLT=%s.", a_v7nn->do_berr ? "yes" :
	    "no");

	a_v7nn->sicy_map = map_map(a_v7nn->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(crate_select), MAP_POKE_REG(crate_select), 0);

	status = MAP_READ(a_v7nn->sicy_map, status_1);
	if (0 != (status & ST1_AMNESIA)) {
		MAP_WRITE(a_v7nn->sicy_map, geo_address, a_v7nn->module.id);
	}
	a_v7nn->geo = 0x1f & MAP_READ(a_v7nn->sicy_map, geo_address);
	LOGF(verbose)(LOGL, "GEO = %u.", a_v7nn->geo);
	crate_module_remap_id(a_crate, a_v7nn->module.id, a_v7nn->geo);

	/* Reset, after GEO. */
	MAP_WRITE(a_v7nn->sicy_map, bit_set_1, BS1_SOFTW_RESET);
	MAP_WRITE(a_v7nn->sicy_map, bit_clear_1,
	    BS1_SELECT_ADDRESS |
	    BS1_SOFTW_RESET);

	SERIALIZE_IO;

	revision = MAP_READ(a_v7nn->sicy_map, firmware_revision);
	LOGF(info)(LOGL, "Firmware revision = %d.%d (0x%04x).",
	    (0xff00 & revision) >> 8, 0xff & revision, revision);

	/* Enable BERR/SIGBUS to end DMA readout. */
	MAP_WRITE(a_v7nn->sicy_map, control_1,
	    !CT1_BLKEND |
	    !CT1_PROG_RESET |
	    (a_v7nn->do_berr ? CT1_BERR_ENABLE : 0) |
	    CT1_ALIGN64);

	MAP_WRITE(a_v7nn->sicy_map, event_counter_reset, 0);
	SERIALIZE_IO;
	a_v7nn->counter_parse =
	    a_v7nn->module.event_counter.value =
	    event_counter_get(a_v7nn);

	if (KW_NOBLT != a_v7nn->blt_mode) {
		a_v7nn->dma_map = map_map(a_v7nn->address, 0x1000,
		    a_v7nn->blt_mode, 1, 0,
		    MAP_POKE_REG(crate_select),
		    MAP_POKE_REG(crate_select), 0);
	}
	LOGF(info)(LOGL, NAME" init_slow }");
}

uint32_t
caen_v7nn_parse_data(struct CaenV7nnModule *a_v7nn, struct EventConstBuffer
    const *a_event_buffer, int a_do_auto_pedestals)
{
	uint32_t const *p32, *end;
	uint32_t header_fixed, data_fixed, eob_fixed;
	uint32_t ms, result = 0;
	unsigned count, i;
	int do_auto_pedestals;

	LOGF(spam)(LOGL, NAME" parse_data(ptr=%p,bytes=%"PRIz") {",
	    a_event_buffer->ptr, a_event_buffer->bytes);

	/* SiCy readout already parses data for ACVT. */
/*	if (KW_NOBLT == a_v7nn->blt_mode &&
	    !crate_do_shadow(a_crate)) {
		goto caen_v7nn_parse_data_end;
	}*/

	do_auto_pedestals = a_do_auto_pedestals && a_v7nn->do_auto_pedestals;
	if (0 != (a_event_buffer->bytes % sizeof(uint32_t))) {
		log_error(LOGL, "Data size not 32-bit aligned.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto caen_v7nn_parse_data_end;
	}
	header_fixed = a_v7nn->geo << 27 | 0x02000000;
	data_fixed = a_v7nn->geo << 27 | 0x00000000;
	eob_fixed = a_v7nn->geo << 27 | 0x04000000;
	p32 = a_event_buffer->ptr;
	end = p32 + a_event_buffer->bytes / 4;
	while (end != p32 && DMA_FILLER == *p32) {
		++p32;
	}
	ms = a_v7nn->module.crate_counter->value;
	for (;;) {
		uint32_t u32, counter;
		if (end < p32) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Parsed beyond limit, confused.");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto caen_v7nn_parse_data_end;
		}
		if (end == p32) {
			break;
		}
		u32 = *p32;
		if (header_fixed != (0xff00c000 & u32)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Header corrupt");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto caen_v7nn_parse_data_end;
		}
		count = (0x3f00 & u32) >> 8;
		++p32;
		/* Check data + EOB. */
		if (!MEMORY_CHECK(*a_event_buffer, &p32[count])) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Event buffer=%"PRIz"B too small for module "
			    "data count=%u.",
			    a_event_buffer->bytes, count);
			result = CRATE_READOUT_FAIL_DATA_MISSING;
			goto caen_v7nn_parse_data_end;
		}
		if (do_auto_pedestals) {
			for (i = 0; count > i; ++i) {
				unsigned channel, value;

				u32 = *p32;
				if (data_fixed != (0xff000000 & u32)) {
					module_parse_error(LOGL,
					    a_event_buffer, p32,
					    "Data corrupt");
					result =
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v7nn_parse_data_end;
				}
				channel = (0x001f0000 & u32) >> 16;
				value = 0x00000fff & u32;
				module_pedestal_add(&a_v7nn->module.
				    pedestal.array[channel], value);
				++p32;
			}
		} else {
			p32 += count;
		}
		u32 = *p32;
		if (eob_fixed != (0xff000000 & u32)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "EOB corrupt");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto caen_v7nn_parse_data_end;
		}
		counter = u32;
		/*
		 * Data event counter conditions =
		 *  "counter_parse < counter < ms"
		 *
		 * -) Single event mode:
		 *    'ms' increments by 1 for every readout (in a working
		 *    setup...) and every acquired counter updates
		 *    'counter_parse', so the condition is always strict if we
		 *    don't write empty events (see Bit Set 2).
		 *
		 * -) Multi event mode:
		 *    'ms' increases by some N for every readout and
		 *    'counter_parse' updates for each event, so again as
		 *    strict as possible.
		 *
		 * -) Shadow mode:
		 *    The # of events in the buffer can grow almost
		 *    arbitrarily, so we can only check a range, in the end is
		 *    the same as multi event mode.
		 *
		 * Because of wrap-around we have no sign and need a cut-off,
		 * 1e4 is <~0.1% of the full range.
		 */
		if (1e4 > COUNTER_VALUE(a_v7nn->counter_parse - counter) ||
		    1e4 > COUNTER_VALUE(counter - ms)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Parse counter=0x%06x < "
			    "counter=0x%06x < ms=0x%06x condition failed.",
			    a_v7nn->counter_parse, counter, ms);
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto caen_v7nn_parse_data_end;
		}
		a_v7nn->counter_parse = counter;
		++p32;
	}

caen_v7nn_parse_data_end:
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}

uint32_t
caen_v7nn_readout(struct Crate *a_crate, struct CaenV7nnModule *a_v7nn, struct
    EventBuffer *a_event_buffer)
{
	uint32_t *outp;
	uint32_t geo_mask, result, u32;
	unsigned event_diff;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");

	outp = a_event_buffer->ptr;
	result = 0;

	event_diff = COUNTER_DIFF_RAW(*a_v7nn->module.crate_counter,
	     a_v7nn->module.crate_counter_prev);

	geo_mask = a_v7nn->geo << 27;

	/* Move data from module. */
	if (KW_NOBLT == a_v7nn->blt_mode) {
		double t_0;
		unsigned data_counter, event, header_count;
		int expect_header, no_data_wait;

		/*
		 * The SiCy approach is a little gnarly because checking the
		 * data words is the only way to know if the FIFO is complete
		 * or not, which is needed to support ACVT. 1 SiCy > a few CPU
		 * operations typically anyhow.
		 */
		data_counter = 0;
		event = 0;
		header_count = 0;
		expect_header = 1;
		no_data_wait = 0;
		t_0 = -1.0;
		for (;;) {
			double t;

			u32 = MAP_READ(a_v7nn->sicy_map, output_buffer);
			if (0x06000000 == (0x07000000 & u32)) {
				goto caen_v7nn_readout_poll;
			}
			if (!MEMORY_CHECK(*a_event_buffer, outp)) {
				result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
				goto caen_v7nn_readout_done;
			}
			*outp++ = u32;
			if (expect_header) {
				if ((0x02000000 | geo_mask) != (0xff00c000 &
				    u32)) {
					log_error(LOGL, "Expected header, "
					    "got 0x%08x.", u32);
					result |=
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v7nn_readout_done;
				}
				header_count = ((0x3f00 & u32) >> 8) + 1;
				data_counter = header_count;
				/* Check space for header + data + footer. */
				if (!MEMORY_CHECK(*a_event_buffer,
				    &outp[data_counter])) {
					log_error(LOGL, "Destination buffer "
					    "too small.");
					result |=
					    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
					goto caen_v7nn_readout_done;
				}
				expect_header = 0;
			} else {
				uint32_t eob_counter;

				if (0 == data_counter--) {
					log_error(LOGL, "Header count=%u "
					    "too small for data.",
					    header_count);
					result |=
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v7nn_readout_done;
				}
				if (geo_mask == (0xff000000 & u32)) {
					continue;
				}
				if ((0x04000000 | geo_mask) != (0xff000000 &
				    u32)) {
					log_error(LOGL, "Expected EOB, got "
					    "0x%08x.", u32);
					result |=
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v7nn_readout_done;
				}
				if (0 != data_counter) {
					log_error(LOGL, "Header count=%u "
					    "too large for data.",
					    header_count);
					result |=
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto caen_v7nn_readout_done;
				}
				/*
				 * EOB contains the counter before
				 * incrementing.
				 */
				eob_counter = COUNTER_VALUE(u32);
				++event;
				if (event_diff == event) {
					if (eob_counter !=
					    a_v7nn->module.event_counter.value)
					{
						log_error(LOGL, "EOB event "
						    "counter=%u != trigger "
						    "counter=%u.",
						    eob_counter,
						    a_v7nn->module.event_counter.value);
						result |=
						    CRATE_READOUT_FAIL_DATA_CORRUPT;
						goto caen_v7nn_readout_done;
					}
					break;
				}
				expect_header = 1;
			}
			continue;
caen_v7nn_readout_poll:
			t = time_getd();
			if (0 > t_0) {
				t_0 = t;
			} else if (NO_DATA_TIMEOUT < t - t_0) {
				log_error(LOGL, "Data timeout (%gs).",
				    NO_DATA_TIMEOUT);
				result |= CRATE_READOUT_FAIL_DATA_MISSING;
				goto caen_v7nn_readout_done;
			}
			sched_yield();
			++no_data_wait;
		}
		if (0 < no_data_wait) {
			crate_acvt_grow(a_crate);
		}
	} else {
		uintptr_t bytes;
		int ret = 0;
		int bad_blt_return = 0;

#if BROKEN_BLT_RETURN_VAL
		bad_blt_return = 1;
#endif

		/* TODO: Parse BLT:ed data for ACVT. */
		if (crate_acvt_has(a_crate)) {
			log_die(LOGL, "ACVT for CAEN modules currently "
			    "unsupported.");
		}
		/*
		 * Max words per event should be number of channels + header
		 * and footer.
		 */
		bytes = event_diff * (a_v7nn->number_of_channels + 2) *
		    sizeof(uint32_t);
		outp = map_align(outp, &bytes, a_v7nn->blt_mode, DMA_FILLER);
		if (!MEMORY_CHECK(*a_event_buffer, &outp[bytes / sizeof *outp
		    - 1])) {
			log_error(LOGL, "Max event size too big for "
			    "output.");
			result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto caen_v7nn_readout_done;
		}
		if (a_v7nn->do_berr) {
			/* Only reset buffer if BLT cannot be trusted */
#if MAP_BLT_RETURN_BROKEN
			unsigned words;
			uint32_t *p32;
			/* +1 to place marker word at buffer end */
			words = bytes / sizeof(uint32_t) + 1;
			for (p32 = outp; 0 != words; --words) {
				*p32++ = 0x06000000;
			}
#endif
			ret = map_blt_read_berr(a_v7nn->dma_map, 0, outp,
			    bytes);
		} else {
			uint32_t *p32;
			unsigned words;

			/* Reset full buffer, blt can finish anywhere. */
			words = bytes / sizeof(uint32_t) + 1;
			for (p32 = outp; 0 != words; --words) {
				*p32++ = 0x06000000;
			}
			/* Ask for one word more which will be no-valid. */
			ret = map_blt_read(a_v7nn->dma_map, 0, outp,
					     bytes + sizeof(uint32_t));
		}
		if (-1 == ret || 0 > ret) {
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto caen_v7nn_readout_done;
		} else {
			bytes = (uintptr_t)ret;
		}

		if (!a_v7nn->do_berr || bad_blt_return) {
			uint32_t const *p32;
			unsigned i, words;

			/*
			 * Search for first no-valid datum.
			 * Binary search not possible because the BLT can
			 * introduce valid data after a no-valid datum.
			 */
			words = bytes / sizeof *outp;
			for (p32 = outp, i = 0;
			    0x06000000 != (0x07000000 & *p32);
			    ++p32, ++i)
			{
				if (i == words) {
					log_error(LOGL, "No-valid datum not "
					    "found.");
					result |=
					    CRATE_READOUT_FAIL_DATA_CORRUPT;
					/* Include all data for inspection. */
					outp += words;
					goto caen_v7nn_readout_done;
				}
			}
			bytes = i * sizeof(uint32_t);
		}

		outp += bytes / sizeof *outp;
	}

caen_v7nn_readout_done:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout }");
	return result;
}

void
caen_v7nn_readout_dt(struct CaenV7nnModule *a_v7nn)
{
	LOGF(spam)(LOGL, NAME" readout_dt {");
	a_v7nn->module.event_counter.value = event_counter_get(a_v7nn);
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x) }",
	    a_v7nn->module.event_counter.value);
}

uint32_t
caen_v7nn_readout_shadow(struct CaenV7nnModule *a_v7nn, struct EventBuffer
    *a_event_buffer)
{
	uint32_t *outp;
	uint32_t const *end;
	uint32_t result, eob, eob_fixed;

	outp = a_event_buffer->ptr;
	result = 0;

	end = (void *)((uintptr_t)a_event_buffer->ptr +
	    a_event_buffer->bytes);
	if (KW_NOBLT == a_v7nn->blt_mode) {
		for (;;) {
			uint32_t u32;

			u32 = MAP_READ(a_v7nn->sicy_map, output_buffer);
			if (0x06000000 == (0x07000000 & u32)) {
				break;
			}
			if (outp >= end) {
				log_error(LOGL, "Too much data!");
				result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
				break;
			}
			*outp++ = u32;
		}
	} else {
		uint32_t *p32;
		uint32_t words;
		uintptr_t bytes;
		int ret;

		/*
		 * Max words per event should be number of channels + header
		 * and footer.
		 */
		bytes = 32 * (a_v7nn->number_of_channels + 2) * sizeof *outp;
		outp = map_align(outp, &bytes, a_v7nn->blt_mode, DMA_FILLER);
		if (!MEMORY_CHECK(*a_event_buffer, &outp[bytes / sizeof *outp
		    - 1])) {
			log_error(LOGL, "Max event size too big for "
			    "output.");
			result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto caen_v7nn_readout_shadow_done;
		}
		words = bytes / sizeof *outp + 1;
		for (p32 = outp; 0 != words; --words) {
			*p32++ = 0x06000000;
		}
		ret = map_blt_read(a_v7nn->dma_map, 0, outp, bytes);
		if (-1 == ret) {
		}
		words = bytes / sizeof *outp;
		/* TODO: We should do these tricks outside of DT. */
		for (p32 = outp; 0 != words; --words, ++p32) {
			if (0x06000000 == (0x07000000 & *p32)) {
				break;
			}
		}
		bytes = (uintptr_t)p32 - (uintptr_t)outp;
	}
	eob = outp[-1];
	eob_fixed = a_v7nn->geo << 27 | 0x04000000;
	if (eob_fixed == (0xff000000 & eob)) {
		a_v7nn->module.shadow.data_counter_value =
		    COUNTER_VALUE(eob);
	}
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
caen_v7nn_readout_shadow_done:
	return result;
}

void
caen_v7nn_use_pedestals(struct CaenV7nnModule *a_v7nn)
{
	struct Pedestal const *pedestal_array;
	uint16_t *threshold_array;
	size_t i;

	if (!a_v7nn->do_auto_pedestals) {
		return;
	}
	LOGF(spam)(LOGL, NAME" use_pedestals {");
	pedestal_array = a_v7nn->module.pedestal.array;
	ASSERT(size_t, PRIz, a_v7nn->number_of_channels, ==,
	    a_v7nn->module.pedestal.array_len);
	MALLOC(threshold_array, a_v7nn->number_of_channels);
	for (i = 0; a_v7nn->number_of_channels > i; ++i) {
		threshold_array[i] = pedestal_array[i].threshold;
	}
	threshold_set(a_v7nn, threshold_array, a_v7nn->number_of_channels);
	free(threshold_array);
	LOGF(spam)(LOGL, NAME" use_pedestals }");
}

void
caen_v7nn_zero_suppress(struct CaenV7nnModule *a_v7nn, int a_yes)
{
	LOGF(spam)(LOGL, NAME" zero_suppress {");
	if (a_yes) {
		MAP_WRITE(a_v7nn->sicy_map, bit_clear_2,
		    BS2_LOW_THRESHOLD_INCL);
	} else {
		MAP_WRITE(a_v7nn->sicy_map, bit_set_2,
		    BS2_LOW_THRESHOLD_INCL);
	}
	LOGF(spam)(LOGL, NAME" zero_suppress }");
}

uint32_t
event_counter_get(struct CaenV7nnModule const *a_v7nn)
{
	uint32_t counter, h, l;

	LOGF(spam)(LOGL, NAME" counter_get {");
	h = MAP_READ(a_v7nn->sicy_map, event_counter_h);
	l = MAP_READ(a_v7nn->sicy_map, event_counter_l);
	counter = ((0x00ff & h) << 16) | l;
	LOGF(spam)(LOGL, NAME" counter_get(0x%08x) }", counter);
	return counter;
}

void
threshold_set(struct CaenV7nnModule *a_v7nn, uint16_t const
    *a_threshold_array, size_t a_threshold_num)
{
	size_t i;
	unsigned threshold_shift;
	uint16_t threshold_stride;
	uint16_t bs2_fine;

	LOGF(verbose)(LOGL, NAME" threshold_set {");
	assert(a_v7nn->number_of_channels == a_threshold_num);
	if (KW_CAEN_V785N == a_v7nn->child_type) {
		threshold_stride = 2;
	} else {
		threshold_stride = 1;
	}
	threshold_shift = 1;
	bs2_fine = BS2_FINE_THRESHOLD;
	for (i = 0; a_threshold_num > i; ++i) {
		if (512 <= a_threshold_array[i]) {
			threshold_shift = 4;
			bs2_fine = 0;
		}
	}
	MAP_WRITE(a_v7nn->sicy_map, bit_set_2, bs2_fine);
	MAP_WRITE(a_v7nn->sicy_map, bit_clear_2,
	    bs2_fine ^ BS2_FINE_THRESHOLD);
	for (i = 0; a_threshold_num > i; ++i) {
		uint16_t channel_offset;

		channel_offset = 0 < a_threshold_array[i] ? 1 : 0;
		MAP_WRITE(a_v7nn->sicy_map, thresholds(i * threshold_stride),
		    0 == ((1 << i) & a_v7nn->channel_enable) ? 0x0100 :
		    (a_threshold_array[i] >> threshold_shift) +
		    channel_offset);
	}
	LOGF(verbose)(LOGL, NAME" threshold_set }");
}
