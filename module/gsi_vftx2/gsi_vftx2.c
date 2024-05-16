/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Bastian Löher
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

#include <module/gsi_vftx2/gsi_vftx2.h>
#include <math.h>
#include <sched.h>
#include <module/gsi_vftx2/internal.h>
#include <module/gsi_vftx2/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <nurdlib/serialio.h>
#include <util/bits.h>
#include <util/endian.h>
#include <util/time.h>

#define NAME "Gsi Vftx2"
#define NO_DATA_TIMEOUT 1.0

MODULE_PROTOTYPES(gsi_vftx2);

uint32_t
gsi_vftx2_check_empty(struct Module *a_module)
{
	struct GsiVftx2Module *vftx2;
	uint32_t status, result;

	/* Why has this been commented out?
	   return 0;
	   */

	LOGF(spam)(LOGL, NAME" check_empty {");
	MODULE_CAST(KW_GSI_VFTX2, vftx2, a_module);
	status = MAP_READ(vftx2->sicy_map, fifo_status);
	result = 0 == (0x1ff0 & status) ? 0 :
	    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	LOGF(spam)(LOGL, NAME" check_empty(0x%08x) }", result);

	return result;
}

struct Module *
gsi_vftx2_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiVftx2Module *vftx2;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(vftx2);
	vftx2->module.event_max = 1;

	vftx2->channel_num = config_get_block_param_int32(a_block, 0);
	if (16 != vftx2->channel_num &&
	    32 != vftx2->channel_num) {
		log_die(LOGL, NAME" create: Channel-num=%u/0x%08x must be 16 "
		    "or 32.", vftx2->channel_num, vftx2->channel_num);
	}
	vftx2->address = config_get_block_param_int32(a_block, 1);
	LOGF(verbose)(LOGL, "Channels=%u address=0x%08x.", vftx2->channel_num,
	    vftx2->address);

	/* The VFTX2 has no event counter. */
	vftx2->module.event_counter.mask = 0;

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)vftx2;
}

void
gsi_vftx2_deinit(struct Module *a_module)
{
	struct GsiVftx2Module *vftx2;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_GSI_VFTX2, vftx2, a_module);
	map_unmap(&vftx2->sicy_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
gsi_vftx2_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
gsi_vftx2_get_map(struct Module *a_module)
{
	struct GsiVftx2Module *vftx2;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_GSI_VFTX2, vftx2, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return vftx2->sicy_map;
}

void
gsi_vftx2_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(0, 7),
		BITS_MASK(24, 31),
		0xab << 24)
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
gsi_vftx2_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct ModuleGate gate;
	struct GsiVftx2Module *vftx2;
	uint32_t invert_mask;
	uint32_t trigger_window;
	unsigned cpld_setting;
	uint32_t future_bit, gate_start;
	int32_t gate_end;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_GSI_VFTX2, vftx2, a_module);

	/* CPLD = input types. */
	cpld_setting = 0;
	{
		enum Keyword const c_kw[] = {KW_LEMO, KW_ECL};
		cpld_setting |= KW_LEMO == CONFIG_GET_KEYWORD(
		    a_module->config, KW_TRIGGER_INPUT, c_kw) ? 0 : 1;
	}
	{
		enum Keyword const c_kw[] = {KW_EXTERNAL, KW_INTERNAL};
		cpld_setting |= KW_INTERNAL == CONFIG_GET_KEYWORD(
		    a_module->config, KW_CLOCK_INPUT, c_kw) ? 0 : 2;
	}
	LOGF(verbose)(LOGL, "CPLD setting=0x%08x.", cpld_setting);
	MAP_WRITE(vftx2->sicy_map, cpld, cpld_setting);

	/* Trigger window / gate. */
	{
		double const c_t0 = ((1 << 12) - 1) * 5.0;
		double const c_t1 = ((1 << 11) - 1) * 5.0;

		module_gate_get(&gate, a_module->config, -c_t0, c_t1, 0.0,
		    c_t0 + c_t1);
	}
	gate_start = ceil(-gate.time_after_trigger_ns / 5.0);
	gate_end = ceil((gate.time_after_trigger_ns + gate.width_ns) / 5.0);
	future_bit = 0x8000;
	if (0 > gate_end) {
		gate_end = -gate_end;
		future_bit = 0;
	}
	trigger_window = (gate_start << 16) | future_bit | gate_end;
	LOGF(verbose)(LOGL, "Trigger window=0x%08x (%g..%g)ns.",
	    trigger_window, -(int)gate_start * 5.0, -(int)gate_end * 5.0);
	MAP_WRITE(vftx2->sicy_map, trigger_window, trigger_window);

	/* Channel inversion masking. */
	invert_mask = config_get_bitmask(a_module->config, KW_CHANNEL_INVERT,
	    0, vftx2->channel_num - 1);
	LOGF(verbose)(LOGL, "Invert mask=0x%08x.", invert_mask);
	MAP_WRITE(vftx2->sicy_map, channel_invert, invert_mask);

	/*
	 * Original f_user uses undocumented registers here... 0x20 and 0x24.
	 */

	/* Channel enable masking. */
	vftx2->channel_mask = config_get_bitmask(a_module->config,
	    KW_CHANNEL_ENABLE, 0, vftx2->channel_num - 1);
	LOGF(verbose)(LOGL, "Channel mask=0x%08x.", vftx2->channel_mask);
	MAP_WRITE(vftx2->sicy_map, channel_enable, vftx2->channel_mask);

	/* Empty FIFO. */
	{
		uint32_t status, hit_num, i;

		status = MAP_READ(vftx2->sicy_map, fifo_status);
		hit_num = (status & 0x1ff0) >> 4;
		for (i = 0; i < hit_num; ++i) {
			uint32_t dumdum;

			dumdum = MAP_READ(vftx2->sicy_map, data_fifo);
			(void)dumdum;
		}
	}

	/* Ikimashou! */
	MAP_WRITE(vftx2->sicy_map, allow_new_trigger, 0);
	MAP_WRITE(vftx2->sicy_map, allow_new_trigger, 1);
	MAP_WRITE(vftx2->sicy_map, trigger_enable, 0);
	MAP_WRITE(vftx2->sicy_map, trigger_enable, vftx2->channel_mask);
	MAP_WRITE(vftx2->sicy_map, start_reset, 1);

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
gsi_vftx2_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiVftx2Module *vftx2;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_GSI_VFTX2, vftx2, a_module);

	vftx2->sicy_map = map_map(vftx2->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(channel_enable), MAP_POKE_REG(channel_enable), 0);

	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
gsi_vftx2_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_vftx2_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	uint32_t const *p;
	uint32_t const *end;
	uint32_t result, u32;
	unsigned count, i;

	(void)a_crate;
	(void)a_module;
	(void)a_do_pedestals;

	LOGF(spam)(LOGL, NAME" parse_data {");

	result = 0;
	if (0 != (a_event_buffer->bytes % sizeof(uint32_t))) {
		log_error(LOGL, NAME" parse_data data_size not 32-bit "
		    "aligned.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto gsi_vftx2_parse_data_end;
	}

	p = a_event_buffer->ptr;
	end = p + a_event_buffer->bytes / sizeof(uint32_t);
	u32 = *p;
	if (0xab000000 != (0xff000000 & u32)) {
		log_error(LOGL, NAME" parse_data header 1 corrupt.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto gsi_vftx2_parse_data_end;
	}

	count = (0x0003fe00 & u32) >> 9;
	++p;
	if (0 < count) {
		if (end <= p) {
			log_error(LOGL, NAME" parse_data header 2 missing.");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_vftx2_parse_data_end;
		}
		u32 = *p;
		if (0x800000aa != (0x9f0007ff & u32)) {
			log_error(LOGL, NAME" parse_data header 2 corrupt.");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_vftx2_parse_data_end;
		}
		++p;
	}
	for (i = 1; count > i; ++i) {
		if (end <= p) {
			log_error(LOGL, NAME" parse_data unexpected end of "
			    "data.");
			result = CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_vftx2_parse_data_end;
		}
		u32 = *p;
		if (0x00000000 != (0x80000000 & u32)) {
			log_error(LOGL, NAME" parse_data data corrupt.");
			result = CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_vftx2_parse_data_end;
		}
		++p;
	}

gsi_vftx2_parse_data_end:
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}

uint32_t
gsi_vftx2_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiVftx2Module *vftx2;
	uint32_t *outp;
	double t_0;
	uint32_t result, status;
	unsigned i, hit_num;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");
	result = 0;
	MODULE_CAST(KW_GSI_VFTX2, vftx2, a_module);

	for (t_0 = -1;;) {
		double t;

		status = MAP_READ(vftx2->sicy_map, fifo_status);
		SERIALIZE_IO;
		if (0 != (0x1ff0 & status)) {
			LOGF(spam)(LOGL, "Status = %08x.", status);
			break;
		}
		t = time_getd();
		if (0 > t_0) {
			t_0 = t;
		} else if (t_0 + NO_DATA_TIMEOUT < t) {
			log_error(LOGL, "Data timeout (>%gs,status=0x%08x).",
			    NO_DATA_TIMEOUT, status);
			result |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_vftx2_readout_done;
		}
		sched_yield();
	}
	if (0 < t_0) {
		crate_acvt_grow(a_crate);
	}

	outp = a_event_buffer->ptr;
	LOGF(spam)(LOGL, "status=0x%08x", status);
	hit_num = (status & 0x1ff0) >> 4;
	/* NOTE: 1 + hit_num words. */
	if (!MEMORY_CHECK(*a_event_buffer, &outp[hit_num])) {
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto gsi_vftx2_readout_done;
	}
	*outp++ = 0xab000000 | (status << 5) | a_module->id;
	for (i = 0; i < hit_num; ++i) {
		*outp++ = MAP_READ(vftx2->sicy_map, data_fifo);
	}
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);

	MAP_WRITE(vftx2->sicy_map, allow_new_trigger, 0);
	MAP_WRITE(vftx2->sicy_map, allow_new_trigger, 1);
	MAP_WRITE(vftx2->sicy_map, trigger_enable, 0);
	MAP_WRITE(vftx2->sicy_map, trigger_enable, vftx2->channel_mask);

gsi_vftx2_readout_done:
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

uint32_t
gsi_vftx2_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	LOGF(spam)(LOGL, NAME" readout_dt.");
	return 0;
}

void
gsi_vftx2_setup_(void)
{
	MODULE_SETUP(gsi_vftx2, 0);
}
