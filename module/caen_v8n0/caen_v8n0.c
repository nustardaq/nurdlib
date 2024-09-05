/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2024
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

#include <module/caen_v8n0/internal.h>
#include <sched.h>
#include <module/caen_v8n0/offsets.h>
#include <module/map/map.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/fmtmod.h>
#include <util/time.h>

#define NAME "Caen v8n0"
#define NO_DATA_TIMEOUT 1.0

enum {
	CONTROL_TRIGGER_RANDOM   = 0x01,
	CONTROL_TRIGGER_PERIODIC = 0x02,
	CONTROL_BERR_ENABLE      = 0x10,
	CONTROL_AUTO_RESET       = 0x80
};

static uint32_t	scaler_get(struct Module *, void *, struct Counter *)
	FUNC_RETURNS;
static void	scaler_parse(struct Crate *, struct ConfigBlock *, char const
    *, struct CaenV8n0Module *);

void
caen_v8n0_create(struct Crate *a_crate, struct ConfigBlock *a_block, struct
    CaenV8n0Module *a_v8n0, enum Keyword a_child_type)
{
	LOGF(verbose)(LOGL, NAME" create {");

	a_v8n0->child_type = a_child_type;

	a_v8n0->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", a_v8n0->address);

	a_v8n0->paux = config_get_boolean(a_block, KW_PAUX);
	LOGF(verbose)(LOGL, "PAUX = %s.", a_v8n0->paux ? "Yes" : "No");

	MODULE_SCALER_PARSE(a_crate, a_block, a_v8n0, scaler_parse);

	LOGF(verbose)(LOGL, NAME" create }");
}

void
caen_v8n0_deinit(struct CaenV8n0Module *a_v8n0)
{
	LOGF(info)(LOGL, NAME" deinit {");
	map_unmap(&a_v8n0->sicy_map);
	map_unmap(&a_v8n0->dma_map);
	LOGF(info)(LOGL, NAME" deinit }");
}

struct Map *
caen_v8n0_get_map(struct CaenV8n0Module *a_v8n0)
{
	LOGF(verbose)(LOGL, NAME" get_map.");
	return a_v8n0->sicy_map;
}

void
caen_v8n0_init_slow(struct CaenV8n0Module *a_v8n0)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_MBLT,
		KW_FF,
		KW_NOBLT
	};
	uint16_t revision;

	LOGF(info)(LOGL, NAME" init_slow {");

	a_v8n0->sicy_map = map_map(a_v8n0->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(dummy16), MAP_POKE_REG(dummy16), 0);

	if (!a_v8n0->paux) {
		MAP_WRITE(a_v8n0->sicy_map, geo, a_v8n0->module.id);
	}
	/* TODO: Report potentially new ID to the crate! */
	a_v8n0->geo = 0x1f & MAP_READ(a_v8n0->sicy_map, geo);
	LOGF(verbose)(LOGL, "GEO = %u.", a_v8n0->geo);

	MAP_WRITE(a_v8n0->sicy_map, module_reset, 0);
	MAP_WRITE(a_v8n0->sicy_map, software_clear, 0);

	SERIALIZE_IO;

	revision = MAP_READ(a_v8n0->sicy_map, firmware);
	LOGF(info)(LOGL, "Firmware revision = %d.%d (0x%02x).",
	    (0xf0 & revision) >> 4, 0x0f & revision, 0xff & revision);

	SERIALIZE_IO;

	a_v8n0->blt_mode = CONFIG_GET_KEYWORD(a_v8n0->module.config,
	    KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "BLT mode=%s.",
	    keyword_get_string(a_v8n0->blt_mode));

	if (KW_NOBLT != a_v8n0->blt_mode) {
		a_v8n0->dma_map = map_map(a_v8n0->address, 0x1000,
		    a_v8n0->blt_mode, 1, 0,
		    MAP_POKE_REG(dummy16), MAP_POKE_REG(dummy16), 0);
	}

	LOGF(info)(LOGL, NAME" init_slow }");
}

uint32_t
scaler_get(struct Module *a_module, void *a_data, struct Counter *a_counter)
{
	struct CaenV8n0Module *v8n0;
	unsigned ch_i;

	LOGF(spam)(LOGL, NAME" scaler_get {");
	/* TODO: This ain't pretty... */
	v8n0 = (void *)a_module;
	ch_i = (uintptr_t)a_data;
	a_counter->value = MAP_READ(v8n0->sicy_map, counter(ch_i));
	a_counter->mask = ~0;
	LOGF(spam)(LOGL, NAME" scaler_get(0x%08x) }", a_counter->value);
	return 0;
}

void
scaler_parse(struct Crate *a_crate, struct ConfigBlock *a_block, char const
    *a_name, struct CaenV8n0Module *a_v8n0)
{
	unsigned ch;

	LOGF(verbose)(LOGL, NAME" scaler_parse(cr=%s,name=%s) {",
	    crate_get_name(a_crate), a_name);
	ch = config_get_int32(a_block, KW_CHANNEL, CONFIG_UNIT_NONE, 0, 31);
	crate_scaler_add(a_crate, a_name, &a_v8n0->module,
	    (void *)(uintptr_t)ch, scaler_get);
	LOGF(verbose)(LOGL, NAME" scaler_parse }");
}
