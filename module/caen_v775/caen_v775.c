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

#include <module/caen_v775/caen_v775.h>
#include <math.h>
#include <module/caen_v775/internal.h>
#include <module/caen_v775/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>

#define NAME "Caen v775"
#define BS2_INVALID_INCL 0x0020
#define BS2_NEVER_EMPTY  0x1000
#define BS2_COMMON_STOP  0x0400
#define BS2_BITS         (BS2_INVALID_INCL |\
    BS2_NEVER_EMPTY |\
    BS2_COMMON_STOP)

MODULE_PROTOTYPES(caen_v775);
static uint32_t	caen_v775_readout_shadow(struct Crate *, struct Module *,
    struct EventBuffer *) FUNC_RETURNS;
static void	caen_v775_zero_suppress(struct Module *, int);
#if NCONF_mMAP_bCMVLC
static void	caen_v775_cmvlc_init(struct Module *,
    struct cmvlc_stackcmdbuf *, int);
static uint32_t caen_v775_cmvlc_fetch_dt(struct Module *,
    const uint32_t *, uint32_t, uint32_t *);
static uint32_t caen_v775_cmvlc_fetch(struct Crate *, struct
    Module *, struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *);
#endif

uint32_t
caen_v775_check_empty(struct Module *a_module)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	return caen_v7nn_check_empty(&v775->v7nn);
}

struct Module *
caen_v775_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV775Module *v775;

	LOGF(verbose)(LOGL, NAME" create {");

	(void)a_crate;
	MODULE_CREATE(v775);
	/* 34 words/event, 32 events, manual p. 16. */
	v775->v7nn.module.event_max = 32;
	caen_v7nn_create(a_block, &v775->v7nn, KW_CAEN_V775);

	/* TODO: Neh, this should not even get here. */
	if (v775->v7nn.do_auto_pedestals) {
		log_die(LOGL, "You cannot enable auto pedestals on a TDC, "
		    "do you know what you are doing?");
	}

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)v775;
}

void
caen_v775_deinit(struct Module *a_module)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	caen_v7nn_deinit(&v775->v7nn);
}

void
caen_v775_destroy(struct Module *a_module)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	caen_v7nn_destroy(&v775->v7nn);
}

struct Map *
caen_v775_get_map(struct Module *a_module)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	return caen_v7nn_get_map(&v775->v7nn);
}

void
caen_v775_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	caen_v7nn_get_signature(a_array, a_num);
}

int
caen_v775_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV775Module *v775;
	uint16_t bs2;
	unsigned fsr, time_range;

	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	caen_v7nn_init_fast(a_crate, &v775->v7nn);

	bs2 = BS2_NEVER_EMPTY;
	if (!config_get_boolean(a_module->config, KW_SUPPRESS_INVALID)) {
		bs2 |= BS2_INVALID_INCL;
	}
	if (!config_get_boolean(a_module->config, KW_COMMON_START)) {
		bs2 |= BS2_COMMON_STOP;
	}
	MAP_WRITE(v775->v7nn.sicy_map, bit_set_2, bs2 & BS2_BITS);
	MAP_WRITE(v775->v7nn.sicy_map, bit_clear_2, (~bs2) & BS2_BITS);

	time_range = config_get_int32(a_module->config, KW_TIME_RANGE,
	    CONFIG_UNIT_NS, 140, 1200);
	LOGF(verbose)(LOGL, "Range=%u ns.", time_range);
	fsr = ceil(4e3 * 8.9 / time_range);
	MAP_WRITE(v775->v7nn.sicy_map, full_scale_range, fsr);
	LOGF(verbose)(LOGL, "Full scale range = %u, resolution = %u ps.",
	    fsr, fsr / 4);

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v775_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV775Module *v775;

	LOGF(info)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	caen_v7nn_init_slow(a_crate, &v775->v7nn);

	MAP_WRITE(v775->v7nn.sicy_map, crate_select, 0);

	LOGF(info)(LOGL, NAME" init_slow }");
	return 1;
}

void
caen_v775_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v775_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct CaenV775Module *v775;

	(void)a_crate;
	(void)a_do_pedestals;
	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	return caen_v7nn_parse_data(&v775->v7nn, a_event_buffer, 0);
}

uint32_t
caen_v775_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	return caen_v7nn_readout(a_crate, &v775->v7nn, a_event_buffer);
}

uint32_t
caen_v775_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV775Module *v775;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	caen_v7nn_readout_dt(&v775->v7nn);
	return 0;
}

uint32_t
caen_v775_readout_shadow(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	struct CaenV775Module *v775;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	return caen_v7nn_readout_shadow(&v775->v7nn, a_event_buffer);
}

#if NCONF_mMAP_bCMVLC
void
caen_v775_cmvlc_init(struct Module *a_module,
    struct cmvlc_stackcmdbuf *a_stack, int a_dt)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	caen_v7nn_cmvlc_init(&v775->v7nn, a_stack, a_dt);
}

uint32_t
caen_v775_cmvlc_fetch_dt(struct Module *a_module,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	return caen_v7nn_cmvlc_fetch_dt(&v775->v7nn,
	    a_in_buffer, a_in_remain, a_in_used);
}

uint32_t
caen_v775_cmvlc_fetch(struct Crate *a_crate,
    struct Module *a_module, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	return caen_v7nn_cmvlc_fetch(a_crate, &v775->v7nn,
	    a_event_buffer, a_in_buffer, a_in_remain, a_in_used);
}
#endif

void
caen_v775_setup_(void)
{
	MODULE_SETUP(caen_v775, 0);
	MODULE_CALLBACK_BIND(caen_v775, readout_shadow);
	MODULE_CALLBACK_BIND(caen_v775, zero_suppress);
#if NCONF_mMAP_bCMVLC
	MODULE_CALLBACK_BIND(caen_v775, cmvlc_init);
	MODULE_CALLBACK_BIND(caen_v775, cmvlc_fetch_dt);
	MODULE_CALLBACK_BIND(caen_v775, cmvlc_fetch);
#endif
}

void
caen_v775_zero_suppress(struct Module *a_module, int a_yes)
{
	struct CaenV775Module *v775;

	MODULE_CAST(KW_CAEN_V775, v775, a_module);
	caen_v7nn_zero_suppress(&v775->v7nn, a_yes);
}
