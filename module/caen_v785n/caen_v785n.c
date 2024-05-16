/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024
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

#include <module/caen_v785n/caen_v785n.h>
#include <module/caen_v785n/internal.h>

#define NAME "Caen v785n"

MODULE_PROTOTYPES(caen_v785n);
static void	caen_v785n_use_pedestals(struct Module *);
static void	caen_v785n_zero_suppress(struct Module *, int);

uint32_t
caen_v785n_check_empty(struct Module *a_module)
{
	struct CaenV785NModule *v785n;
	uint32_t result;

	LOGF(spam)(LOGL, NAME" check_empty {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	result = caen_v7nn_check_empty(&v785n->v7nn);
	LOGF(spam)(LOGL, NAME" check_empty(0x%08x) }", result);
	return result;
}

struct Module *
caen_v785n_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV785NModule *v785n;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(v785n);
	/* 34 words/event, 32 events, manual p. 16. */
	v785n->v7nn.module.event_max = 32;
	caen_v7nn_create(a_block, &v785n->v7nn, KW_CAEN_V785N);
	MODULE_CREATE_PEDESTALS(v785n, caen_v785n, 16); /* 785N is the 16 channel version of V785 */
	LOGF(verbose)(LOGL, NAME" create }");
	return &v785n->v7nn.module;
}

void
caen_v785n_deinit(struct Module *a_module)
{
	struct CaenV785NModule *v785n;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	caen_v7nn_deinit(&v785n->v7nn);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
caen_v785n_destroy(struct Module *a_module)
{
	struct CaenV785NModule *v785n;

	LOGF(verbose)(LOGL, NAME" destroy {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	caen_v7nn_destroy(&v785n->v7nn);
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
caen_v785n_get_map(struct Module *a_module)
{
	struct CaenV785NModule *v785n;

	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	return caen_v7nn_get_map(&v785n->v7nn);
}

void
caen_v785n_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	caen_v7nn_get_signature(a_array, a_num);
}

int
caen_v785n_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV785NModule *v785n;

	LOGF(verbose)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	caen_v7nn_init_fast(a_crate, &v785n->v7nn);
	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v785n_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV785NModule *v785n;

	LOGF(verbose)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	caen_v7nn_init_slow(a_crate, &v785n->v7nn);
	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
caen_v785n_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v785n_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct CaenV785NModule *v785n;
	uint32_t result;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" parse_data {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	result = caen_v7nn_parse_data(&v785n->v7nn, a_event_buffer,
	    a_do_pedestals);
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}

uint32_t
caen_v785n_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV785NModule *v785n;
	uint32_t result;

	LOGF(spam)(LOGL, NAME" readout {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	result = caen_v7nn_readout(a_crate, &v785n->v7nn, a_event_buffer);
	LOGF(spam)(LOGL, NAME" readout*0x%08x) }", result);
	return result;
}

uint32_t
caen_v785n_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV785NModule *v785n;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	caen_v7nn_readout_dt(&v785n->v7nn);
	LOGF(spam)(LOGL, NAME" readout }");
	return 0;
}

void
caen_v785n_setup_(void)
{
	MODULE_SETUP(caen_v785n, 0);
	MODULE_CALLBACK_BIND(caen_v785n, use_pedestals);
	MODULE_CALLBACK_BIND(caen_v785n, zero_suppress);
}

void
caen_v785n_use_pedestals(struct Module *a_module)
{
	struct CaenV785NModule *v785n;

	LOGF(spam)(LOGL, NAME" use_pedestals {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	caen_v7nn_use_pedestals(&v785n->v7nn);
	LOGF(spam)(LOGL, NAME" use_pedestals }");
}

void
caen_v785n_zero_suppress(struct Module *a_module, int a_yes)
{
	struct CaenV785NModule *v785n;

	LOGF(spam)(LOGL, NAME" zero_suppress {");
	MODULE_CAST(KW_CAEN_V785N, v785n, a_module);
	caen_v7nn_zero_suppress(&v785n->v7nn, a_yes);
	LOGF(spam)(LOGL, NAME" zero_suppress }");
}
