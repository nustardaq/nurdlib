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

#include <module/caen_v785/caen_v785.h>
#include <module/caen_v785/internal.h>

#define NAME "Caen v785"

MODULE_PROTOTYPES(caen_v785);
static void	caen_v785_use_pedestals(struct Module *);
static void	caen_v785_zero_suppress(struct Module *, int);

uint32_t
caen_v785_check_empty(struct Module *a_module)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	return caen_v7nn_check_empty(&v785->v7nn);
}

struct Module *
caen_v785_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV785Module *v785;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(v785);
	/* 34 words/event, 32 events, manual p. 16. */
	v785->v7nn.module.event_max = 32;
	caen_v7nn_create(a_block, &v785->v7nn, KW_CAEN_V785);
	MODULE_CREATE_PEDESTALS(v785, caen_v785, 32);
	LOGF(verbose)(LOGL, NAME" create }");
	return &v785->v7nn.module;
}

void
caen_v785_deinit(struct Module *a_module)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	caen_v7nn_deinit(&v785->v7nn);
}

void
caen_v785_destroy(struct Module *a_module)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	caen_v7nn_destroy(&v785->v7nn);
}

struct Map *
caen_v785_get_map(struct Module *a_module)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	return caen_v7nn_get_map(&v785->v7nn);
}

void
caen_v785_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	caen_v7nn_get_signature(a_array, a_num);
}

int
caen_v785_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	caen_v7nn_init_fast(a_crate, &v785->v7nn);
	return 1;
}

int
caen_v785_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	caen_v7nn_init_slow(a_crate, &v785->v7nn);
	return 1;
}

void
caen_v785_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v785_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct CaenV785Module *v785;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	return caen_v7nn_parse_data(&v785->v7nn, a_event_buffer,
	    a_do_pedestals);
}

uint32_t
caen_v785_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	return caen_v7nn_readout(a_crate, &v785->v7nn, a_event_buffer);
}

uint32_t
caen_v785_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV785Module *v785;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	caen_v7nn_readout_dt(&v785->v7nn);
	return 0;
}

void
caen_v785_setup_(void)
{
	MODULE_SETUP(caen_v785, 0);
	MODULE_CALLBACK_BIND(caen_v785, use_pedestals);
	MODULE_CALLBACK_BIND(caen_v785, zero_suppress);
}

void
caen_v785_use_pedestals(struct Module *a_module)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	caen_v7nn_use_pedestals(&v785->v7nn);
}

void
caen_v785_zero_suppress(struct Module *a_module, int a_yes)
{
	struct CaenV785Module *v785;

	MODULE_CAST(KW_CAEN_V785, v785, a_module);
	caen_v7nn_zero_suppress(&v785->v7nn, a_yes);
}
