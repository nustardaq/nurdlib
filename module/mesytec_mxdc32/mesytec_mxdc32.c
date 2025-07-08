/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2025
 * Bastian Löher
 * Michael Munch
 * Oliver Papst
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

#include <module/mesytec_mxdc32/internal.h>
#include <sched.h>
#include <unistd.h>
#include <module/map/map.h>
#include <module/mesytec_mxdc32/internal.h>
#include <module/mesytec_mxdc32/offsets.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/sigbus.h>
#include <util/time.h>

#define NAME "Mesytec Mxdc32"

#define COUNTER_VALUE(data) (0x3fffffff & (data))

static uint32_t	get_event_counter(struct MesytecMxdc32Module const *)
	FUNC_RETURNS;

uint32_t
get_event_counter(struct MesytecMxdc32Module const *a_mxdc32)
{
	uint32_t lo, hi, counter;

	lo = MAP_READ(a_mxdc32->sicy_map, evctr_lo);
	SERIALIZE_IO;
	hi = MAP_READ(a_mxdc32->sicy_map, evctr_hi);
	counter = lo | hi << 16;
	return counter;
}

uint32_t
mesytec_mxdc32_check_empty(struct MesytecMxdc32Module *a_mxdc32)
{
	uint16_t buffer_data_length;
	uint32_t result;

	LOGF(spam)(LOGL, NAME" check_empty {");
	buffer_data_length = MAP_READ(a_mxdc32->sicy_map, buffer_data_length);
	LOGF(spam)(LOGL, "buffer_data_length = %u.", buffer_data_length);
	result = 0 == buffer_data_length ? 0 :
	    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	LOGF(spam)(LOGL, NAME" check_empty(0x%08x) }", result);
	return result;
}

void
mesytec_mxdc32_create(struct ConfigBlock const *a_block, struct
    MesytecMxdc32Module *a_mxdc32)
{
	LOGF(verbose)(LOGL, NAME" create {");

	a_mxdc32->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", a_mxdc32->address);

	/* This is the data event counter mask, necessary for shadow. */
	a_mxdc32->module.event_counter.mask = BITS_MASK_TOP(29);

	/* Sleep for some init operations, but can be skipped for tests. */
	a_mxdc32->do_sleep = 1;

	LOGF(verbose)(LOGL, NAME" create }");
}

void
mesytec_mxdc32_deinit(struct MesytecMxdc32Module *a_mxdc32)
{
	LOGF(verbose)(LOGL, NAME" deinit {");
	map_unmap(&a_mxdc32->sicy_map);
	map_unmap(&a_mxdc32->dma_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
mesytec_mxdc32_destroy(struct MesytecMxdc32Module *a_mxdc32)
{
	(void)a_mxdc32;
}

struct Map *
mesytec_mxdc32_get_map(struct MesytecMxdc32Module *a_mxdc32)
{
	LOGF(verbose)(LOGL, NAME" get_map.");
	return a_mxdc32->sicy_map;
}

void
mesytec_mxdc32_init_fast(struct Crate *a_crate, struct MesytecMxdc32Module
    *a_mxdc32, int a_supported_bank_ops)
{
	enum Keyword banks_operation;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");

	LOGF(info)(LOGL, "Firmware revision = 0x%04x.",
	    MAP_READ(a_mxdc32->sicy_map, firmware_revision));

	MAP_WRITE(a_mxdc32->sicy_map, module_id, a_mxdc32->module.id);

	if (KW_MBLT == a_mxdc32->blt_mode) {
		/* 64-bit transfers. */
		MAP_WRITE(a_mxdc32->sicy_map, data_len_format, 3);
		a_mxdc32->datum_bytes = sizeof(uint64_t);
	} else {
		MAP_WRITE(a_mxdc32->sicy_map, data_len_format, 2);
		a_mxdc32->datum_bytes = sizeof(uint32_t);
	}

	if (0 != a_supported_bank_ops) {
		/*
		 * BL: This needs to be defined here, otherwise the array
		 * decays to a pointer and cannot be sent to LENGTH macro.
		 */
		enum Keyword const c_banks_op_array[] = {
			KW_CONNECTED, KW_INDEPENDENT, KW_TOGGLE};

#define SUPPORTED(op) \
		if ((a_supported_bank_ops & op) == 0) { \
			log_die(LOGL, "bank_operation=%s not supported by " \
			    "this module type.\n", \
	    		    keyword_get_string(banks_operation)); \
		}

		banks_operation = CONFIG_GET_KEYWORD(a_mxdc32->module.config,
		    KW_BANKS_OPERATION, c_banks_op_array);
		LOGF(verbose)(LOGL, "Using banks_operation mode = %s.",
		    keyword_get_string(banks_operation));
		switch (banks_operation) {
		case KW_CONNECTED:
			SUPPORTED(MESYTEC_BANK_OP_CONNECTED);
			MAP_WRITE(a_mxdc32->sicy_map, bank_operation, 0);
			break;
		case KW_INDEPENDENT:
			SUPPORTED(MESYTEC_BANK_OP_INDEPENDENT);
			MAP_WRITE(a_mxdc32->sicy_map, bank_operation, 1);
			break;
		case KW_TOGGLE:
			SUPPORTED(MESYTEC_BANK_OP_TOGGLE);
			MAP_WRITE(a_mxdc32->sicy_map, bank_operation, 3);
			break;
		default:
			log_die(LOGL, "This should not happen! "
			    "bank_operation=%s",
			    keyword_get_string(banks_operation));
		}
	}
#undef SUPPORTED

	/* Treat everything as multi-event and verify counters. */
	MAP_WRITE(a_mxdc32->sicy_map, max_transfer_data, 0);
	MAP_WRITE(a_mxdc32->sicy_map, multievent, 1);

	MAP_WRITE(a_mxdc32->sicy_map, fifo_reset, 0);
	MAP_WRITE(a_mxdc32->sicy_map, readout_reset, 0);
	MAP_WRITE(a_mxdc32->sicy_map, reset_ctr_ab, 3);

	LOGF(info)(LOGL, NAME" init_fast }");
}

void
mesytec_mxdc32_init_slow(struct MesytecMxdc32Module *a_mxdc32)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_FF,
		KW_MBLT,
		KW_NOBLT
	};

	LOGF(info)(LOGL, NAME" init_slow {");

	a_mxdc32->sicy_map = map_map(a_mxdc32->address, MAP_SIZE, KW_NOBLT, 0,
	    0, MAP_POKE_REG(module_id), MAP_POKE_REG(module_id), 0);

	/* BLT mode */
	a_mxdc32->blt_mode = CONFIG_GET_KEYWORD(a_mxdc32->module.config,
	    KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "BLT mode = %s.",
	    keyword_get_string(a_mxdc32->blt_mode));
	if (KW_NOBLT != a_mxdc32->blt_mode) {
		a_mxdc32->dma_map = map_map(a_mxdc32->address, 1026 * 4,
		    a_mxdc32->blt_mode, 0, 1,
		    MAP_POKE_REG(module_id), MAP_POKE_REG(module_id), 0);
	}

	MAP_WRITE(a_mxdc32->sicy_map, start_acq, 0);
	MAP_WRITE(a_mxdc32->sicy_map, soft_reset, 1);
	if (a_mxdc32->do_sleep) {
		time_sleep(0.2);
	}
	SERIALIZE_IO;

	LOGF(info)(LOGL, NAME" init_slow }");
}

uint32_t
mesytec_mxdc32_parse_data(struct Crate *a_crate, struct MesytecMxdc32Module
    *a_mxdc32, struct EventConstBuffer const *a_event_buffer, int
    a_do_pedestals, int a_is_eob_old, int a_skip_event_counter_check)
{
	uint32_t const *end;
	uint32_t const *p32;
	uint32_t count_exp;
	uint32_t header_len_mask, data_sig, data_sig_msk, ch_msk, value_msk;
	uint32_t ext_tstamp_sig;
	uint32_t sample_sig;
	uint32_t result;

	LOGF(spam)(LOGL, NAME" parse_data(ptr=%p,bytes=%"PRIz") {",
	    a_event_buffer->ptr, a_event_buffer->bytes);
	result = 0;
	if (0 != (a_event_buffer->bytes % sizeof(uint32_t))) {
		log_error(LOGL, "Data size not 32-bit aligned.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto mesytec_mxdc32_parse_data_done;
	}
	p32 = a_event_buffer->ptr;
	end = p32 + a_event_buffer->bytes / sizeof(uint32_t);

	/* Compact streaming format (output_format = 4). */
	if (0 && crate_free_running_get(a_crate)) {
		for (; p32 < end;) {
			uint32_t u32, msb, id;

			u32 = *p32;
			msb = u32 >> 30;
			if (0x1 != msb) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Header MSBs corrupt "
				    "(exp=0x1, have=0x%x)",
				    msb);
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
			id = (0x3f000000 & u32) >> 24;
			if (a_mxdc32->module.id != id) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Header ID corrupt "
				    "(exp=%u, have=%u)",
				    a_mxdc32->module.id, id);
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
			++p32;
			u32 = *p32;
			msb = u32 >> 30;
			if (0x3 != msb) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "EoE MSBs corrupt "
				    "(exp=0x3, have=0x%x)",
				    msb);
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
			++p32;
		}
		goto mesytec_mxdc32_parse_data_done;
	}

	ext_tstamp_sig = 0;
	sample_sig = 0;

	/* Prepare some bitmasks. */
	/* TODO: Check bits 12..15? Depends on subtype. */
	if ((KW_MESYTEC_MDPP16SCP == a_mxdc32->module.type) ||
	    (KW_MESYTEC_MDPP16QDC == a_mxdc32->module.type)) {
		header_len_mask = 0x3ff;
		data_sig = 0x10000000;
		data_sig_msk = 0xf0000000;
		ext_tstamp_sig = 0x20000000;
		sample_sig = 0x30000000;
		ch_msk = 0x003f0000;
		value_msk = 0x0000ffff;
	} else if (KW_MESYTEC_MDPP32SCP == a_mxdc32->module.type) {
		header_len_mask = 0x3ff;
		data_sig = 0x10000000;
		data_sig_msk = 0xf0000000;
		ext_tstamp_sig = 0x20000000;
		sample_sig = 0x30000000;
		ch_msk = 0x007f0000;
		value_msk = 0x0000ffff;
	} else if (KW_MESYTEC_VMMR8 == a_mxdc32->module.type){
		header_len_mask = 0xfff;
		data_sig = 0x00000000;
		data_sig_msk = 0xc0000000;
		ch_msk = 0x00000000;
		value_msk = 0x00000000;
	} else if (KW_MESYTEC_MADC32 == a_mxdc32->module.type) {
		header_len_mask = 0x3ff;
		data_sig = 0x04000000;
		data_sig_msk = 0xffc00000;
		ch_msk = 0x001f0000;
		value_msk = 0x00003fff;
	} else if (KW_MESYTEC_MQDC32 == a_mxdc32->module.type) {
		header_len_mask = 0xfff;
		data_sig = 0x04000000;
		data_sig_msk = 0xffc00000;
		ch_msk = 0x001f0000;
		value_msk = 0x00000fff;
	} else if (KW_MESYTEC_MTDC32 == a_mxdc32->module.type) {
		header_len_mask = 0xfff;
		data_sig = 0x04000000;
		data_sig_msk = 0xffc00000;
		ch_msk = 0x001f0000;
		value_msk = 0x0000ffff;
	} else {
		log_die(LOGL,
		    "Internal error! Parsing unsupported for %d=%s.",
		    a_mxdc32->module.type,
		    keyword_get_string(a_mxdc32->module.type));
	}

	/* Gobble DMA start alignment filler. */
	while (end != p32 && DMA_FILLER == *p32) {
		++p32;
	}
	count_exp = a_mxdc32->parse_counter + (a_is_eob_old ? 0 : 1);
	for (;;) {
		uint32_t count, eoe, header;
		unsigned id, len, words;

		if (end < p32) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Parsed beyond limit, confused.");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto mesytec_mxdc32_parse_data_done;
		}
		if (end == p32) {
			break;
		}
		header = *p32;
		/* Work around a bug in the new standard streaming
		 * mode where the module number is placed at the wrong
		 * place in the header (the place of it in compact
		 * streaming mode).  Once that is fixed (and flashed)
		 * in firmware, it would make sense to remove this
		 * outer if-statement and the else-clause.
		 */
		if (!crate_free_running_get(a_crate)) {
			if (0x40000000 != (0xfe000000 & header)) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Header corrupt");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
			id = (0x00ff0000 & header) >> 16;
			if (a_mxdc32->module.id != id) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Module header ID corrupt");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
		}
		else {
			if (0x40000000 != (0xc0000000 & header)) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Streaming header corrupt");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
			id = (0x3f000000 & header) >> 24;
			if (a_mxdc32->module.id != id) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Module streaming header ID corrupt");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
		}
		len = (header & header_len_mask) - 1;
		++p32;
		/* Check data + EOE. */
		if (!MEMORY_CHECK(*a_event_buffer, &p32[len])) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Less data than reported.");
			result = CRATE_READOUT_FAIL_DATA_MISSING;
			goto mesytec_mxdc32_parse_data_done;
		}
		for (words = 0; words < len; ++words, ++p32) {
			uint32_t word;

			word = *p32;
			/* Gobble DMA (MBLT) filler at end of readout. */
			if (0x00000000 == word && len - 1 == words) {
				continue;
			}
			if (ext_tstamp_sig &&
			    ext_tstamp_sig == (data_sig_msk & word)) {
				/* Extended time-stamp. */
				continue;
			}
			if (sample_sig &&
			    sample_sig == (data_sig_msk & word)) {
				/* Sample signature. */
				/* TODO: number of sample words is
				 * stored in 10 lowest bits.
				 * Check that it is not too many, and
				 * check those too.  The all have the
				 * sample signature.
				 */
				continue;
			}
			if (data_sig != (data_sig_msk & word)) {
				module_parse_error(LOGL, a_event_buffer, p32,
				    "Data signature corrupt.");
				result = CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto mesytec_mxdc32_parse_data_done;
			}
			if (a_do_pedestals) {
				unsigned channel;

				channel = (ch_msk & word) >> 16;
				if (!((KW_MESYTEC_MDPP16SCP ==
				       a_mxdc32->module.type) ||
				    (KW_MESYTEC_MDPP16QDC ==
				     a_mxdc32->module.type)) ||
					16 > channel) {
					unsigned value;

					value = value_msk & word;
					module_pedestal_add(
					    &a_mxdc32->module.pedestal.array[
					    channel], value);
				}
			}
		}
		eoe = *p32;
		if (0xc0000000 != (0xc0000000 & eoe)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "EOE corrupt");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto mesytec_mxdc32_parse_data_done;
		}

		count = COUNTER_VALUE(eoe);
		count_exp = COUNTER_VALUE(count_exp);
		if (!a_skip_event_counter_check &&
		    !crate_free_running_get(a_crate) &&
		    count != count_exp) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Event counter=0x%08x mismatch, expected 0x%08x",
			    count, count_exp);
			result = CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH;
			goto mesytec_mxdc32_parse_data_done;
		}
		++count_exp;
		++p32;
	}
mesytec_mxdc32_parse_data_done:
	a_mxdc32->parse_counter = a_mxdc32->module.event_counter.value;
	LOGF(spam)(LOGL, NAME" parse_data(0x%08x) }", result);
	return result;
}

int
mesytec_mxdc32_post_init(struct MesytecMxdc32Module *a_mxdc32)
{
	double init_sleep;

	LOGF(spam)(LOGL, NAME" post_init {");
	init_sleep = mesytec_mxdc32_sleep_get(a_mxdc32);
	time_sleep(init_sleep);
	/* Read counter in post-init, the reset needs a moment. */
	a_mxdc32->parse_counter =
	    a_mxdc32->module.event_counter.value =
	    get_event_counter(a_mxdc32);
	time_sleep(init_sleep);
	MAP_WRITE(a_mxdc32->sicy_map, fifo_reset, 1);
	time_sleep(init_sleep);
	MAP_WRITE(a_mxdc32->sicy_map, start_acq, 1);
	LOGF(spam)(LOGL, NAME" post_init(ctr=0x%08x) }",
	    a_mxdc32->module.event_counter.value);
	return 1;
}

uint32_t
mesytec_mxdc32_readout(struct Crate *a_crate, struct MesytecMxdc32Module
    *a_mxdc32, struct EventBuffer *a_event_buffer, int a_is_eob_old, int
    a_skip_event_counter_check)
{
	uint32_t *outp;
	uint32_t data_size, i, result;
	unsigned unit_size;

	LOGF(spam)(LOGL, NAME" readout {");

	outp = a_event_buffer->ptr;
	result = 0;

	/* Figure out how much data to move. */
	if (a_mxdc32->module.skip_dt) {
		data_size = 1 << 17;
		/* Only used for printing. */
		unit_size = 1;
	} else {
		uint16_t data_len_format;

		data_len_format =
		    MAP_READ(a_mxdc32->sicy_map, data_len_format);
		switch (data_len_format & 0x3) {
			case 0:
			case 1:
			case 2:
			case 3:
				unit_size = 1 << data_len_format;
				break;
			default:
				log_error(LOGL,
				    "Mesytec Mxdc32: Weird data length format"
				    " '%04x'.", data_len_format);
				result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
				goto mesytec_mxdc32_readout_done;
		}
		data_size = a_mxdc32->buffer_data_length * unit_size;
		/* TODO: What about (3&data_size)!=0? */
	}
	LOGF(spam)(LOGL, "data_size = %u bytes.", data_size);
	if (0 == data_size) {
		goto mesytec_mxdc32_readout_done;
	}
	if (!MEMORY_CHECK(*a_event_buffer, &outp[data_size/4 - 1])) {
		log_error(LOGL, "Not enough space for event data, "
		    "module=0x%x*%dB=0x%08xB, event-buffer=0x%"PRIzx"B.",
		    a_mxdc32->buffer_data_length, unit_size,
		    data_size, a_event_buffer->bytes);
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto mesytec_mxdc32_readout_done;
	}

	/* Move data from module. */
	if (KW_NOBLT == a_mxdc32->blt_mode) {
		uint32_t *outp_start;
		uint32_t buffer_data_len, actual_read;

		outp_start = outp;
		/*
		 * TODO: Looks like buffer_data_len is not for complete
		 * events?
		 */
		sigbus_set_ok(1);
		data_size /= 4;
		for (i = 0; data_size > i; ++i) {
			uint32_t u32;

			u32 = MAP_READ(a_mxdc32->sicy_map, data_fifo(0));
			if (sigbus_was()) {
				break;
			}
			*outp++ = u32;
		}
		actual_read = outp - outp_start;
		buffer_data_len =
		    MAP_READ(a_mxdc32->sicy_map, buffer_data_length);
		if (actual_read != data_size) {
			log_error(LOGL, "Readout size mismatch: "
			    "data_buffer_len_before=%uB "
			    "read_until_sigbus=%u "
			    "data_buffer_len_after=%uB.",
			    a_mxdc32->buffer_data_length,
			    actual_read,
			    buffer_data_len);
		}
	} else {
		uintptr_t bytes;
		int ret;

		bytes = data_size;
#define BLT_CHUNK_SIZE (65536+0)
		while (bytes > 0) {
			uintptr_t byt;
			if (bytes > BLT_CHUNK_SIZE) {
				byt = BLT_CHUNK_SIZE;
				bytes -= BLT_CHUNK_SIZE;
			} else {
				byt = bytes;
				bytes = 0;
			}
			outp = map_align(outp, &byt, a_mxdc32->blt_mode,
			    DMA_FILLER);
			ret = map_blt_read_berr(a_mxdc32->dma_map, 0, outp,
			    byt);
			if (-1 == ret) {
				log_error(LOGL, "DMA read failed!");
				result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
				goto mesytec_mxdc32_readout_done;
			}
			outp += ret / sizeof *outp;
			if (0 == outp[-1]) {
				--outp;
			}
		}
	}

	/* Assert EOB event counter. */
	if (!crate_free_running_get(a_crate) &&
	    !a_skip_event_counter_check) {
		uint32_t counter_eob, expected;

		counter_eob = COUNTER_VALUE(outp[-1]);
		expected = COUNTER_VALUE(a_mxdc32->module.event_counter.value
		    + (a_is_eob_old ? -1 : 0));
		if (counter_eob != expected) {
			log_error(LOGL, "Event counter mismatch (EOB=0x%08x, "
			    "expected=0x%08x).", counter_eob, expected);
			result |= CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH;
			goto mesytec_mxdc32_readout_done;
		}
	}

mesytec_mxdc32_readout_done:
	MAP_WRITE(a_mxdc32->sicy_map, readout_reset, 0);
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

uint32_t
mesytec_mxdc32_readout_dt(struct Crate *a_crate, struct MesytecMxdc32Module
    *a_mxdc32)
{
	LOGF(spam)(LOGL, NAME" readout_dt {");
	if (!crate_free_running_get(a_crate)) {
		a_mxdc32->module.event_counter.value =
		    get_event_counter(a_mxdc32);
	}
	a_mxdc32->buffer_data_length =
	    MAP_READ(a_mxdc32->sicy_map, buffer_data_length);
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x,buf_len=0x%08x) }",
	    a_mxdc32->module.event_counter.value,
	    a_mxdc32->buffer_data_length);
	return 0;
}

uint32_t
mesytec_mxdc32_readout_shadow(struct MesytecMxdc32Module *a_mxdc32, struct
    EventBuffer *a_event_buffer, int a_is_eob_old)
{
	uint32_t *outp;
	enum Keyword blt_mode;
	uint32_t result, data_bytes, i, eob;
	unsigned buf_len;

	outp = a_event_buffer->ptr;
	result = 0;

	/*
	 * Keep the same BLT mode throughout this function, it's ok if
	 * different invocations do different things because of async with
	 * init*.
	 */
	blt_mode = a_mxdc32->blt_mode;

	buf_len = MAP_READ(a_mxdc32->sicy_map, buffer_data_length);
	if (0 == buf_len) {
		return result;
	}
	data_bytes = buf_len * a_mxdc32->datum_bytes;
	if (a_event_buffer->bytes < data_bytes) {
		log_error(LOGL, "Too much data for shadow buffer!");
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto mesytec_mxdc32_readout_shadow_done;
	}
	sigbus_set_ok(1);
	if (KW_NOBLT == blt_mode) {
		for (i = 0; i < data_bytes; i += sizeof(uint32_t)) {
			uint32_t u32;

			u32 = MAP_READ(a_mxdc32->sicy_map, data_fifo(0));
			if (sigbus_was()) {
				break;
			}
			*outp++ = u32;
		}
	} else {
		uintptr_t bytes;
		int ret;

		bytes = data_bytes + sizeof(uint32_t);
		outp = map_align(outp, &bytes, blt_mode, DMA_FILLER);
		ret = map_blt_read_berr(a_mxdc32->dma_map, 0, outp, bytes);
		if (-1 == ret) {
/*			log_error(LOGL, "DMA read failed!");
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;*/
			goto mesytec_mxdc32_readout_shadow_done;
		}
if (0) {
	printf("%u: %u\n", a_mxdc32->module.id, data_bytes);
	for (i = 0; i < bytes; i += 4)
		printf(" %08x", outp[i]);
	puts("");
}
		outp += bytes / sizeof *outp;
	}
	eob = outp[-1];
	if (3 == (eob >> 30)) {
		/* TODO: Does this work with external mdpp16scp clock? */
		a_mxdc32->module.shadow.data_counter_value =
		    COUNTER_VALUE(eob + (a_is_eob_old ? 0 : 1));
	}
mesytec_mxdc32_readout_shadow_done:
	sigbus_set_ok(0);
	MAP_WRITE(a_mxdc32->sicy_map, readout_reset, 0);
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	return result;
}

double
mesytec_mxdc32_sleep_get(struct MesytecMxdc32Module *a_mxdc32)
{
	double duration;

	duration = config_get_double(a_mxdc32->module.config, KW_INIT_SLEEP,
	    CONFIG_UNIT_S, 0.0, 5.0);
	if (!a_mxdc32->do_sleep) {
		duration = 0.0;
	}
	return duration;
}

void
mesytec_mxdc32_zero_suppress(struct MesytecMxdc32Module *a_mxdc32, int a_yes)
{
	LOGF(verbose)(LOGL, NAME" zero_suppress {");
	MAP_WRITE(a_mxdc32->sicy_map, ignore_thresholds, a_yes ? 0 : 1);
	LOGF(verbose)(LOGL, NAME" zero_suppress }");
}
