/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2021, 2023-2024
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

#include <module/caen_v965/caen_v965.h>
#include <module/caen_v965/internal.h>
#include <module/caen_v965/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>

#define NAME "Caen v965"

#define IPED_0 (620.0f - 255.0f * 0.5f)

MODULE_PROTOTYPES(caen_v965);
static void	caen_v965_use_pedestals(struct Module *);
static void	caen_v965_zero_suppress(struct Module *, int);
#if NCONF_mMAP_bCMVLC
static void	caen_v965_cmvlc_init(struct Module *,
    struct cmvlc_stackcmdbuf *, int);
static uint32_t caen_v965_cmvlc_fetch_dt(struct Module *,
    const uint32_t *, uint32_t, uint32_t *);
static uint32_t caen_v965_cmvlc_fetch(struct Crate *, struct
    Module *, struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *);
#endif

uint32_t
caen_v965_check_empty(struct Module *a_module)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	return caen_v7nn_check_empty(&v965->v7nn);
}

struct Module *
caen_v965_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV965Module *v965;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(v965);
	/* 34 words/event, 32 events, manual p. 15. */
	v965->v7nn.module.event_max = 32;
	caen_v7nn_create(a_block, &v965->v7nn, KW_CAEN_V965);
	MODULE_CREATE_PEDESTALS(v965, caen_v965, 32);
	LOGF(verbose)(LOGL, NAME" create }");
	return &v965->v7nn.module;
}

void
caen_v965_deinit(struct Module *a_module)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_deinit(&v965->v7nn);
}

void
caen_v965_destroy(struct Module *a_module)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_destroy(&v965->v7nn);
}

struct Map *
caen_v965_get_map(struct Module *a_module)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	return caen_v7nn_get_map(&v965->v7nn);
}

void
caen_v965_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	caen_v7nn_get_signature(a_array, a_num);
}

int
caen_v965_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV965Module *v965;
	unsigned iped;

	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_init_fast(a_crate, &v965->v7nn);

	iped = config_get_int32(a_module->config, KW_IPED, CONFIG_UNIT_NONE,
	    0, 255);
	LOGF(verbose)(LOGL, "IPED=%u ~= %f uA.", iped,
	    caen_v965_iped_to_current(iped));
	MAP_WRITE(v965->v7nn.sicy_map, iped, iped);

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v965_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_init_slow(a_crate, &v965->v7nn);
	return 1;
}

float
caen_v965_iped_to_current(unsigned a_iped)
{
	return IPED_0 + a_iped * 0.5f;
}

void
caen_v965_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v965_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct CaenV965Module *v965;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	return caen_v7nn_parse_data(&v965->v7nn, a_event_buffer,
	    a_do_pedestals);
}

uint32_t
caen_v965_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	return caen_v7nn_readout(a_crate, &v965->v7nn, a_event_buffer);
}

uint32_t
caen_v965_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV965Module *v965;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_readout_dt(&v965->v7nn);
	return 0;
}

#if NCONF_mMAP_bCMVLC
void
caen_v965_cmvlc_init(struct Module *a_module,
    struct cmvlc_stackcmdbuf *a_stack, int a_dt)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_cmvlc_init(&v965->v7nn, a_stack, a_dt);
}

uint32_t
caen_v965_cmvlc_fetch_dt(struct Module *a_module,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	return caen_v7nn_cmvlc_fetch_dt(&v965->v7nn,
	    a_in_buffer, a_in_remain, a_in_used);
}

uint32_t
caen_v965_cmvlc_fetch(struct Crate *a_crate,
    struct Module *a_module, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	return caen_v7nn_cmvlc_fetch(a_crate, &v965->v7nn,
	    a_event_buffer, a_in_buffer, a_in_remain, a_in_used);
}
#endif

void
caen_v965_setup_(void)
{
	MODULE_SETUP(caen_v965, 0);
	MODULE_CALLBACK_BIND(caen_v965, use_pedestals);
	MODULE_CALLBACK_BIND(caen_v965, zero_suppress);
#if NCONF_mMAP_bCMVLC
	MODULE_CALLBACK_BIND(caen_v965, cmvlc_init);
	MODULE_CALLBACK_BIND(caen_v965, cmvlc_fetch_dt);
	MODULE_CALLBACK_BIND(caen_v965, cmvlc_fetch);
#endif
}

void
caen_v965_use_pedestals(struct Module *a_module)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_use_pedestals(&v965->v7nn);
}

void
caen_v965_zero_suppress(struct Module *a_module, int a_yes)
{
	struct CaenV965Module *v965;

	MODULE_CAST(KW_CAEN_V965, v965, a_module);
	caen_v7nn_zero_suppress(&v965->v7nn, a_yes);
}
