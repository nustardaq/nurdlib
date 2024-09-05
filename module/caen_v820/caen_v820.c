/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2021, 2023-2024
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

#include <module/caen_v820/caen_v820.h>
#include <module/caen_v820/internal.h>
#include <module/caen_v820/offsets.h>
#include <module/map/map.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/bits.h>

#define NAME "Caen v820"
#define NO_DATA_TIMEOUT 1.0

enum {
	CONTROL_TRIGGER_RANDOM   = 0x01,
	CONTROL_TRIGGER_PERIODIC = 0x02,
	CONTROL_BERR_ENABLE      = 0x10,
	CONTROL_AUTO_RESET       = 0x80
};

MODULE_PROTOTYPES(caen_v820);

uint32_t
caen_v820_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
caen_v820_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV820Module *v820;

	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(v820);
	v820->v8n0.module.event_max = 1;
	v820->v8n0.module.event_counter.mask = 0;
	caen_v8n0_create(a_crate, a_block, &v820->v8n0, KW_CAEN_V820);
	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)v820;
}

void
caen_v820_deinit(struct Module *a_module)
{
	struct CaenV820Module *v820;

	MODULE_CAST(KW_CAEN_V820, v820, a_module);
	caen_v8n0_deinit(&v820->v8n0);
}

void
caen_v820_destroy(struct Module *a_module)
{
	(void)a_module;
}

struct Map *
caen_v820_get_map(struct Module *a_module)
{
	struct CaenV820Module *v820;

	MODULE_CAST(KW_CAEN_V820, v820, a_module);
	return caen_v8n0_get_map(&v820->v8n0);
}

void
caen_v820_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
caen_v820_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV820Module *v820;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_CAEN_V820, v820, a_module);
	v820->channel_mask = config_get_bitmask(a_module->config,
	    KW_CHANNEL_ENABLE, 0, 31);
	v820->channel_num = bits_get_count(v820->channel_mask);
	LOGF(verbose)(LOGL, "channel_mask = %08x, channel_num = %u",
	    v820->channel_mask, v820->channel_num);
	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v820_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV820Module *v820;
	uint16_t control;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_CAEN_V820, v820, a_module);

	caen_v8n0_init_slow(&v820->v8n0);

	SERIALIZE_IO;

	control =
	    (CONTROL_TRIGGER_RANDOM) |
	    (!CONTROL_TRIGGER_PERIODIC) |
	    (!CONTROL_BERR_ENABLE) |
	    (!CONTROL_AUTO_RESET);
	MAP_WRITE(v820->v8n0.sicy_map, control, control);

	SERIALIZE_IO;

	LOGF(info)(LOGL, NAME" init_slow }");
	return 1;
}

void
caen_v820_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v820_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

uint32_t
caen_v820_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV820Module *v820;
	uint32_t *outp;
	uint32_t result;
	size_t i;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");

	result = 0;
	MODULE_CAST(KW_CAEN_V820, v820, a_module);
	outp = a_event_buffer->ptr;
	if (!MEMORY_CHECK(*a_event_buffer, &outp[v820->channel_num])) {
		log_error(LOGL, "Too much data.");
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto caen_v820_readout_done;
	}
	for (i = 0; 32 > i; ++i) {
		if (0 != (1 & (v820->channel_mask >> i))) {
			*outp++ = MAP_READ(v820->v8n0.sicy_map, counter(i));
		}
	}
caen_v820_readout_done:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

uint32_t
caen_v820_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	return 0;
}

void
caen_v820_setup_(void)
{
	MODULE_SETUP(caen_v820, 0);
}
