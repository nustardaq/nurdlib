/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
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

#include <module/caen_v560/caen_v560.h>
#include <module/caen_v560/internal.h>
#include <module/caen_v560/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/math.h>
#include <util/string.h>

#define NAME "Caen v560"

MODULE_PROTOTYPES(caen_v560);

uint32_t
caen_v560_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
caen_v560_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV560Module *v560;

	LOGF(verbose)(LOGL, NAME" create {");

	(void)a_crate;
	MODULE_CREATE(v560);
	v560->module.event_max = 32; /* no event buffer, arbitrary > 0 */
	v560->address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address=%08x.", v560->address);

	v560->module.event_counter.mask = 0;

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)v560;
}

void
caen_v560_deinit(struct Module *a_module)
{
	struct CaenV560Module *v560;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_CAEN_V560, v560, a_module);
	map_unmap(&v560->sicy_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
caen_v560_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
caen_v560_get_map(struct Module *a_module)
{
	struct CaenV560Module *v560;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_CAEN_V560, v560, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return v560->sicy_map;
}

void
caen_v560_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
caen_v560_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV560Module *v560;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_CAEN_V560, v560, a_module);

	v560->use_veto = config_get_boolean(a_module->config, KW_USE_VETO);
	LOGF(verbose)(LOGL, "use_veto = %s", v560->use_veto ? "yes" : "no");

	MAP_WRITE(v560->sicy_map, scale_clear, 1);
	MAP_WRITE(v560->sicy_map, vme_veto_reset, 1);

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v560_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV560Module *v560;
	uint16_t id;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_CAEN_V560, v560, a_module);

	v560->sicy_map = map_map(v560->address, MAP_SIZE, KW_NOBLT,
	    0, 0, MAP_POKE_REG(fixed_code), MAP_POKE_REG(scale_clear), 0);

	id = MAP_READ(v560->sicy_map, fixed_code);
	if (0xfaf5 != id) {
		log_die(LOGL, "Fixed code=0x%04x != 0xfaf5, module borked?",
		    id);
	}
	id = MAP_READ(v560->sicy_map, manufacturer_module_type);
	LOGF(verbose)(LOGL, "Manufacturer=%02x, module type=%03x (0x%04x).",
	    (0xfc00 & id) >> 10, 0x3ff & id, id);
	id = MAP_READ(v560->sicy_map, version_serial_number);
	LOGF(verbose)(LOGL, "Version=%x, S/N=%03x (0x%04x).",
	    (0xf000 & id) >> 12, 0xfff & id, id);

	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
caen_v560_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v560_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

uint32_t
caen_v560_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV560Module *v560;
	uint32_t *outp;
	uint32_t result = 0;
	int ch;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout {");

	MODULE_CAST(KW_CAEN_V560, v560, a_module);
	outp = a_event_buffer->ptr;

	if (v560->use_veto) {
		MAP_WRITE(v560->sicy_map, vme_veto_set, 1);
	}

	*outp++ = 0xc560c560;
	for (ch = 0; ch < 16; ch++) {
		*outp++ = MAP_READ(v560->sicy_map, counter(ch));
	}

	if (v560->use_veto) {
		MAP_WRITE(v560->sicy_map, vme_veto_reset, 1);
	}

	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

uint32_t
caen_v560_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	LOGF(spam)(LOGL, NAME" readout_dt.");
	return 0;
}

void
caen_v560_setup_(void)
{
	MODULE_SETUP(caen_v560, 0);
}
