/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
 * Håkan T Johansson
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

#include <module/sis_3820_scaler/sis_3820_scaler.h>
#include <module/sis_3820_scaler/internal.h>
#include <module/sis_3820_scaler/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/bits.h>

#define NAME "sis3820_scaler"
#define DMA_FILLER 0xff3820ff

MODULE_PROTOTYPES(sis_3820_scaler);

uint32_t
sis_3820_scaler_check_empty(struct Module *a_module)
{
	struct Sis3820ScalerModule *m;
	uint32_t count;

	LOGF(spam)(LOGL, NAME" check_empty {");
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);
	count = MAP_READ(m->sicy_map, fifo_word_count);
	LOGF(spam)(LOGL, NAME" check_empty(cnt=%u) }", count);
	return 0 == count ? 0 : CRATE_READOUT_FAIL_DATA_TOO_MUCH;
}

struct Module *
sis_3820_scaler_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_MBLT,
		KW_NOBLT
	};
	struct Sis3820ScalerModule *m;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(m);

	/*
	 * Manual p.18: 0x800000..0xfffffc = 8 MiB = 64ki...
	 * Let's cap at 1024 :p Don't forget the custom event header!
	 */
	m->module.event_max = 1024;

	m->address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address = 0x%x", m->address);

	m->blt_mode = CONFIG_GET_KEYWORD(a_block, KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "BLT mode=%s.", keyword_get_string(m->blt_mode));

	m->is_latching = config_get_boolean(a_block, KW_LATCHING);
	if (!m->is_latching) {
		m->module.event_counter.mask = 0;
	}

	LOGF(verbose)(LOGL, NAME"create }");
	return (void *)m;
}

void
sis_3820_scaler_deinit(struct Module *a_module)
{
	struct Sis3820ScalerModule *m;

	LOGF(verbose)(LOGL, NAME"deinit {");
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);
	map_unmap(&m->sicy_map);
	LOGF(verbose)(LOGL, NAME"deinit }");
}

void
sis_3820_scaler_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

void
sis_3820_scaler_memtest(struct Module *a_module, enum Keyword a_memtest_mode)
{
	(void)a_module;
	(void)a_memtest_mode;
	LOGF(verbose)(LOGL, NAME" memtest.");
}

struct Map *
sis_3820_scaler_get_map(struct Module *a_module)
{
	struct Sis3820ScalerModule const *m;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return m->sicy_map;
}

void
sis_3820_scaler_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
sis_3820_scaler_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3820ScalerModule *m;
	uint32_t op_mode;
	uint32_t i;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);

	MAP_WRITE(m->sicy_map, key_reset, 1);

	op_mode = 0;

	/* MODE: 0 = single scalers, 2 = MCS. */
	op_mode |= m->is_latching ? 2 << 28 : 0;

	/* INPUT MODE: 1 = LNE on LEMO 1. */
	op_mode |= 1 << 16;

	/* LNE SOURCE. */
	op_mode |= m->is_latching ? 1 << 4 : 0;

	/* DATA FORMAT: bits 2..3 of reg 0x100. */
	m->data_format = config_get_int32(a_module->config, KW_DATA_FORMAT,
	    CONFIG_UNIT_NONE, 8, 32);
	switch (m->data_format) {
	case 24:
		op_mode |= 1 << 2;
		break;
	case 32:
		break;
	default:
		log_die(LOGL,
		    "sis3820_scaler data format = %u, must be 24 or 32.",
		    m->data_format);
	}

	/* NON CLEARING MODE: never clear. */
	op_mode |= 0x1;

	LOGF(verbose)(LOGL, "op_mode = 0x%08x.", op_mode);
	MAP_WRITE(m->sicy_map, op_mode, op_mode);

	MAP_WRITE(m->sicy_map, key_op_enable, 0x1);

	m->channel_mask = config_get_bitmask(a_module->config,
	    KW_CHANNEL_ENABLE, 0, 31);
	LOGF(verbose)(LOGL, "channel_mask = 0x%08x.", m->channel_mask);
	m->channel_num = 0;
	for (i = 0; i < 32; ++i) {
		if (0 != (1 & (m->channel_mask >> i))) {
			m->channel_map[m->channel_num++] = i;
		}
	}
	MAP_WRITE(m->sicy_map, copy_disable, ~m->channel_mask);
	m->module.event_counter.value = MAP_READ(m->sicy_map, acq_count);

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
sis_3820_scaler_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3820ScalerModule *m;
	uint32_t id_fw, id;

	(void)a_crate;

	LOGF(verbose)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);

	m->sicy_map = map_map(m->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(control_status), MAP_POKE_REG(control_status), 0);

	if (KW_NOBLT != m->blt_mode) {
		m->dma_map = map_map(m->address, 0x1000, m->blt_mode, 1, 0,
		    /* TODO: Why not poke-read firmware reg? */
		    0, 0, 0,
		    0, 0, 0, 0);
	}

	id_fw = MAP_READ(m->sicy_map, id_and_firmware);
	LOGF(verbose)(LOGL, "id_fw = 0x%08x.", id_fw);
	id = id_fw >> 16;
	if (0x3820 != id) {
		log_die(LOGL, NAME" Invalid module id = 0x%04x (0x%08x)!",
		    id, id_fw);
	}
	/* TODO: Check FW! */

	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

uint32_t
sis_3820_scaler_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct Sis3820ScalerModule *m;
	uint32_t const *p32;
	uint32_t result = 0, header, ev_num, ch_num, bytes;

	LOGF(spam)(LOGL, NAME" parse_data {");

	(void)a_crate;
	(void)a_do_pedestals;
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);

	if (a_event_buffer->bytes < sizeof(uint32_t)) {
		log_error(LOGL,
		    NAME" parse_data bytes=%"PRIz", no space for header!",
		    a_event_buffer->bytes);
		result |= CRATE_READOUT_FAIL_DATA_MISSING;
		goto parse_data_done;
	}
	p32 = a_event_buffer->ptr;
	header = *p32++;
	ev_num = (0x000fff00 & header) >> 8;
	ch_num = 0x3f & header;
	bytes = sizeof(uint32_t) * (1 + ev_num * ch_num);
	if (a_event_buffer->bytes != bytes) {
		log_error(LOGL,
		    NAME" parse_data bytes=%"PRIz", but expected %u!",
		    a_event_buffer->bytes, bytes);
		result |= CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto parse_data_done;
	}

	if (24 == m->data_format) {
		uint32_t ev_i;

		for (ev_i = 0; ev_i < ev_num; ++ev_i) {
		uint32_t ch_i;

			for (ch_i = 0; ch_i < ch_num; ++ch_i) {
				unsigned ch;

				/* User bits should be 0, test them too. */
				ch = *p32++ >> 24;
				if (ch > 31) {
					log_error(LOGL, NAME" parse_data bad "
					    "data header=0x%02x, should be < "
					    "0x20!", ch);
					result |= CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto parse_data_done;
				} else if (0 == (m->channel_mask & (1 << ch)))
				{
					log_error(LOGL, NAME" parse_data data"
					    " channel %u not enabled, channel"
					    " mask=0x%08x!", ch,
					    m->channel_mask);
					result |= CRATE_READOUT_FAIL_DATA_CORRUPT;
					goto parse_data_done;
				}
			}
		}
	}

parse_data_done:
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}

uint32_t
sis_3820_scaler_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct Sis3820ScalerModule *m;
	uint32_t *outp, result;
	unsigned i, event_diff, word_num;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");
	result = 0;
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);
	outp = a_event_buffer->ptr;

	event_diff = COUNTER_DIFF_RAW(*a_module->crate_counter,
	    a_module->crate_counter_prev);
	word_num = event_diff * m->channel_num;
	LOGF(spam)(LOGL, "event_diff=%u ch=%u -> word_num=%u.", event_diff,
	    m->channel_num, word_num);
	if (!MEMORY_CHECK(*a_event_buffer, &outp[word_num])) {
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto sis_3820_scaler_readout_done;
	}

	/*
	 * CREATE HEADER:
	 *  31:27 = module id (rank in the config file)
	 *  23:23 = 0 = 24b, 1 = 32b
	 *  22:22 = 0 = raw single scaler readout, 1 = latching (MCS)
	 *  19:8  = # events
	 *   5:0  = # channels
	 */
	*outp++ =
	    m->module.id << 27 |
	    (24 == m->data_format ? 0 : 1) << 23 |
	    m->is_latching << 22 |
	    event_diff << 8 |
	    m->channel_num;

	/* MOVE DATA FROM MODULE. */
	if (m->is_latching) {
		uint32_t fifo_count;

		fifo_count = MAP_READ(m->sicy_map, fifo_word_count);
		if (fifo_count != word_num) {
			result |= fifo_count < word_num ?
			    CRATE_READOUT_FAIL_DATA_MISSING :
			    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto sis_3820_scaler_readout_done;
		}
		if (KW_NOBLT == m->blt_mode) {
			for (i = 0; i < fifo_count; ++i) {
				*outp++ = MAP_READ(m->sicy_map, fifo);
			}
		} else {
			int ret;
			size_t bytes;

			bytes = fifo_count * sizeof(uint32_t);
			outp = map_align(outp, &bytes, m->blt_mode,
			    DMA_FILLER);
			if (!MEMORY_CHECK(*a_event_buffer, &outp[bytes /
			    sizeof *outp - 1])) {
				result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
				goto sis_3820_scaler_readout_done;
			}
			ret = map_blt_read_berr(m->dma_map, 0, outp, bytes);
			/* TODO: Care about broken return val. */
			if (-1 == ret) {
				result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
				goto sis_3820_scaler_readout_done;
			}
			outp += bytes / sizeof *outp;
		}
	} else {
		for (i = 0; i < m->channel_num; ++i) {
			*outp++ = MAP_READ(m->sicy_map,
			    counter_register(m->channel_map[i]));
		}
	}

	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);

sis_3820_scaler_readout_done:
	LOGF(spam)(LOGL, NAME" readout }");
	return result;
}

uint32_t
sis_3820_scaler_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3820ScalerModule *m;
	uint32_t ret = 0;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_SIS_3820_SCALER, m, a_module);
	if (m->is_latching) {
		m->module.event_counter.value =
		    MAP_READ(m->sicy_map, acq_count);
	}
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x) }",
	    m->module.event_counter.value);
	return ret;
}

void
sis_3820_scaler_setup_(void)
{
	MODULE_SETUP(sis_3820_scaler, 0);
}
