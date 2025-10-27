/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
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

#include <module/mesytec_mtdc32/mesytec_mtdc32.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_mtdc32/internal.h>
#include <module/mesytec_mtdc32/offsets.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/bits.h>

#define NAME "Mesytec Mtdc32"

MODULE_PROTOTYPES(mesytec_mtdc32);
static int	mesytec_mtdc32_post_init(struct Crate *, struct Module *);

static uint32_t	mesytec_mtdc32_readout_shadow(struct Crate *, struct Module *,
    struct EventBuffer *) FUNC_RETURNS;
#if NCONF_mMAP_bCMVLC
static void    mesytec_mtdc32_cmvlc_init(struct Module *,
    struct cmvlc_stackcmdbuf *, int);
static uint32_t mesytec_mtdc32_cmvlc_fetch_dt(struct Module *,
    const uint32_t *, uint32_t, uint32_t *);
static uint32_t mesytec_mtdc32_cmvlc_fetch(struct Crate *, struct
    Module *, struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *);
#endif

uint32_t
mesytec_mtdc32_check_empty(struct Module *a_module)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_check_empty(&mtdc32->mxdc32);
}

struct Module *
mesytec_mtdc32_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct MesytecMtdc32Module *mtdc32;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(mtdc32);
	/* Page 10 in the MTDC32 V2.5_01 data sheet. */
	mtdc32->mxdc32.module.event_max = 48640 / (1+32+1+1+1);
	mesytec_mxdc32_create(a_block, &mtdc32->mxdc32);
	LOGF(verbose)(LOGL, NAME" create }");
	return (void *)mtdc32;
}

void
mesytec_mtdc32_deinit(struct Module *a_module)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	mesytec_mxdc32_deinit(&mtdc32->mxdc32);
}

void
mesytec_mtdc32_destroy(struct Module *a_module)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	mesytec_mxdc32_destroy(&mtdc32->mxdc32);
}

struct Map *
mesytec_mtdc32_get_map(struct Module *a_module)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_get_map(&mtdc32->mxdc32);
}

void
mesytec_mtdc32_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(16, 23),
		BITS_MASK(24, 31),
		0x2 << 30)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature }");
}

int
mesytec_mtdc32_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	enum Keyword const c_trigger_input[] = {KW_ECL, KW_NIM};
	struct ModuleGate gate;
	struct MesytecMtdc32Module *mtdc32;
	enum Keyword trigger_input_kw;
	int trigger_input;
	int only_first_hit = 0;
	int pulser_enabled = 0;

	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);

	module_gate_get(&gate, a_module->config, -16384.0, 16384.0, 0.0,
	    16384.0);
	MAP_WRITE(mtdc32->mxdc32.sicy_map, bank0_win_start,
	    gate.time_after_trigger_ns + 16384);
	MAP_WRITE(mtdc32->mxdc32.sicy_map, bank0_win_width, gate.width_ns);
	MAP_WRITE(mtdc32->mxdc32.sicy_map, bank1_win_start,
	    gate.time_after_trigger_ns + 16384);
	MAP_WRITE(mtdc32->mxdc32.sicy_map, bank1_win_width, gate.width_ns);

	{
		double const c_valid_resolution[] =
		    {3.9, 7.8, 15.6, 31.3, 62.5, 125, 250, 500};
		double resolution;
		size_t i;
		uint16_t resolution_code;

		resolution = config_get_double(a_module->config,
		    KW_RESOLUTION, CONFIG_UNIT_PS, 0,
		    c_valid_resolution[LENGTH(c_valid_resolution) - 1]);
		for (i = 0; LENGTH(c_valid_resolution) - 1 > i; ++i) {
			if (resolution < c_valid_resolution[i + 1]) {
				break;
			}
		}
		resolution_code = i + 2;
		LOGF(verbose)(LOGL, NAME" Wanted resolution=%.1f ps, chose "
		    "%.1f ps (0x%04x).", resolution, c_valid_resolution[i],
		    resolution_code);
		MAP_WRITE(mtdc32->mxdc32.sicy_map, tdc_resolution,
		    resolution_code);
	}

	trigger_input_kw = CONFIG_GET_KEYWORD(a_module->config,
	    KW_TRIGGER_INPUT, c_trigger_input);
	trigger_input = KW_NIM == trigger_input_kw ? 0 : 1;
	LOGF(info)(LOGL, "Trigger input=%s=%u.",
	    keyword_get_string(trigger_input_kw), trigger_input);
	MAP_WRITE(mtdc32->mxdc32.sicy_map, trig_select, trigger_input);

	/* pulser setup:
	 * Enabled pulser sends a signal to:
	 *   trig0 + ch0
	 *   ...
	 *   trig0 + ch31
	 *   trig1 + ch0
	 *   ...
	 *   trig1 + ch31
	 */
	pulser_enabled = config_get_boolean(a_module->config,
	    KW_PULSER_ENABLED);
	LOGF(verbose)(LOGL, "Pulser status=%s.", pulser_enabled ? "on" :
	    "off");
	MAP_WRITE(mtdc32->mxdc32.sicy_map, pulser_status,
	    pulser_enabled ? 3 : 0);

	/* multi-hit setup */
	only_first_hit = config_get_boolean(a_module->config,
	    KW_ONLY_FIRST_HIT);
	if (only_first_hit) {
		MAP_WRITE(mtdc32->mxdc32.sicy_map, first_hit, 0x3);
	} else {
		MAP_WRITE(mtdc32->mxdc32.sicy_map, first_hit, 0);
	}
	LOGF(info)(LOGL, "Only first hit=%s.",
	    only_first_hit ? "yes" : "no");

	/*
	 * The MTDC will by default write empty events.
	 * The requires that the multiplicity is a least 1.
	 */
	/* MAP_WRITE(mtdc32->mxdc32.sicy_map, low_limit0 = 1; */
	/* MAP_WRITE(mtdc32->mxdc32.sicy_map, low_limit1 = 1; */

	mesytec_mxdc32_init_fast(a_crate, &mtdc32->mxdc32,
	    MESYTEC_BANK_OP_CONNECTED | MESYTEC_BANK_OP_INDEPENDENT);

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_mtdc32_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMtdc32Module *mtdc32;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	mesytec_mxdc32_init_slow(&mtdc32->mxdc32);
	return 1;
}

void
mesytec_mtdc32_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_mtdc32_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecMtdc32Module *mtdc32;

	(void)a_do_pedestals;
	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_parse_data(a_crate, &mtdc32->mxdc32,
	    a_event_buffer, 0, 1, 0);
}

int
mesytec_mtdc32_post_init(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMtdc32Module *mtdc32;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_post_init(&mtdc32->mxdc32);
}

uint32_t
mesytec_mtdc32_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_readout(a_crate, &mtdc32->mxdc32,
	    a_event_buffer, 1, 0);
}

uint32_t
mesytec_mtdc32_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_readout_dt(a_crate, &mtdc32->mxdc32);
}

uint32_t
mesytec_mtdc32_readout_shadow(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	struct MesytecMtdc32Module *mtdc32;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_readout_shadow(&mtdc32->mxdc32, a_event_buffer,
	    0);
}

#if NCONF_mMAP_bCMVLC
void
mesytec_mtdc32_cmvlc_init(struct Module *a_module,
    struct cmvlc_stackcmdbuf *a_stack, int a_dt)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	mesytec_mxdc32_cmvlc_init(&mtdc32->mxdc32, a_stack, a_dt);
}

uint32_t
mesytec_mtdc32_cmvlc_fetch_dt(struct Module *a_module,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_cmvlc_fetch_dt(&mtdc32->mxdc32,
	    a_in_buffer, a_in_remain, a_in_used);
}

uint32_t
mesytec_mtdc32_cmvlc_fetch(struct Crate *a_crate,
    struct Module *a_module, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecMtdc32Module *mtdc32;

	MODULE_CAST(KW_MESYTEC_MTDC32, mtdc32, a_module);
	return mesytec_mxdc32_cmvlc_fetch(a_crate, &mtdc32->mxdc32,
	    a_event_buffer, a_in_buffer, a_in_remain, a_in_used);
}
#endif

void
mesytec_mtdc32_setup_(void)
{
	MODULE_SETUP(mesytec_mtdc32, 0);
	MODULE_CALLBACK_BIND(mesytec_mtdc32, post_init);
	MODULE_CALLBACK_BIND(mesytec_mtdc32, readout_shadow);
#if NCONF_mMAP_bCMVLC
	MODULE_CALLBACK_BIND(mesytec_mtdc32, cmvlc_init);
	MODULE_CALLBACK_BIND(mesytec_mtdc32, cmvlc_fetch_dt);
	MODULE_CALLBACK_BIND(mesytec_mtdc32, cmvlc_fetch);
#endif
}
