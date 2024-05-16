/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2024
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

#include <module/sis_3801/sis_3801.h>
#include <module/map/map.h>
#include <module/sis_3801/internal.h>
#include <module/sis_3801/offsets.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/bits.h>

#define NAME "sis3801"

MODULE_PROTOTYPES(sis_3801);

uint32_t
sis_3801_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
sis_3801_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct Sis3801Module *m;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(m);
	m->module.event_max = 1;
	m->address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address = %08x", m->address);
	LOGF(verbose)(LOGL, NAME" create }");
	return (void *)m;
}

void
sis_3801_deinit(struct Module *a_module)
{
	struct Sis3801Module *m;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_SIS_3801, m, a_module);
	map_unmap(&m->sicy_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
sis_3801_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

void
sis_3801_memtest(struct Module *a_module, enum Keyword a_memtest_mode)
{
	(void)a_module;
	(void)a_memtest_mode;
	LOGF(verbose)(LOGL, NAME" memtest.");
}

struct Map *
sis_3801_get_map(struct Module *a_module)
{
	struct Sis3801Module *m;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_SIS_3801, m, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return m->sicy_map;
}

void
sis_3801_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

#define DEFAULT_CLOCK_FREQ_KHZ 100000

int
sis_3801_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3801Module *m;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_SIS_3801, m, a_module);

	MAP_WRITE(m->sicy_map, reset, 1);
	MAP_WRITE(m->sicy_map, clear, 1);
	MAP_WRITE(m->sicy_map, control, 0x10000);
	m->channel_enable = config_get_bitmask(m->module.config,
	    KW_CHANNEL_ENABLE, 0, 31);
	LOGF(verbose)(LOGL, "channel_enable=0x%08x", m->channel_enable);
	MAP_WRITE(m->sicy_map, copy_disable, ~m->channel_enable);
	MAP_WRITE(m->sicy_map, enable_next_clock_logic, 1);
	m->is_running = 0;

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
sis_3801_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3801Module *m;

	(void)a_crate;

	LOGF(verbose)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_SIS_3801, m, a_module);

	/* Map single cycle. */
	m->sicy_map = map_map(m->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(prescaler_factor),
	    MAP_POKE_REG(prescaler_factor), 0);

	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

uint32_t
sis_3801_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	LOGF(verbose)(LOGL, NAME" parse_data {");
	++a_module->event_counter.value;
	LOGF(verbose)(LOGL, NAME" parse_data }");
	return 0;
}

uint32_t
sis_3801_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct Sis3801Module *m;
	uint32_t *outp;
	uint32_t result;

	(void)a_crate;

	LOGF(debug)(LOGL, NAME" readout {");
	result = 0;
	outp = a_event_buffer->ptr;

	MODULE_CAST(KW_SIS_3801, m, a_module);

	if (m->is_running) {
		unsigned i;

		if (0 != (0x100 & MAP_READ(m->sicy_map, status))) {
			result = CRATE_READOUT_FAIL_DATA_MISSING;
		}
		for (i = 0; i < 32; ++i) {
			if (1 & (m->channel_enable >> i)) {
				*outp++ = MAP_READ(m->sicy_map, fifo(0));
			}
		}
	}
	if (0 == (0x100 & MAP_READ(m->sicy_map, status))) {
		result = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto sis_3801_readout_done;
	}
	m->is_running = 1;

sis_3801_readout_done:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(debug)(LOGL, NAME" readout }");
	return result;
}

uint32_t
sis_3801_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	LOGF(debug)(LOGL, NAME" readout_dt.");
	return 0;
}

void
sis_3801_setup_(void)
{
	MODULE_SETUP(sis_3801, 0);
}
