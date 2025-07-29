/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2025
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

#include <module/caen_v1290/caen_v1290.h>
#include <module/caen_v1290/internal.h>
#include <nurdlib/log.h>

#define NAME "Caen v1290"

MODULE_PROTOTYPES(caen_v1290);

static int	caen_v1290_register_list_pack(struct Module *, struct
    PackerList *);

uint32_t
caen_v1290_check_empty(struct Module *a_module)
{
	struct CaenV1290Module *v1290;

	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	return caen_v1n90_check_empty(&v1290->v1n90);
}

struct Module *
caen_v1290_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV1290Module *v1290;

	(void)a_crate;
	MODULE_CREATE(v1290);
	caen_v1n90_create(a_block, &v1290->v1n90);
	return &v1290->v1n90.module;
}

void
caen_v1290_deinit(struct Module *a_module)
{
	struct CaenV1290Module *v1290;

	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	caen_v1n90_deinit(&v1290->v1n90);
}

void
caen_v1290_destroy(struct Module *a_module)
{
	struct CaenV1290Module *v1290;

	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	caen_v1n90_destroy(&v1290->v1n90);
}

struct Map *
caen_v1290_get_map(struct Module *a_module)
{
	struct CaenV1290Module *v1290;

	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	return caen_v1n90_get_map(&v1290->v1n90);
}

void
caen_v1290_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	caen_v1n90_get_signature(a_array, a_num);
}

int
caen_v1290_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1290Module *v1290;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	caen_v1n90_init_fast(&v1290->v1n90, KW_CAEN_V1290);
	return 1;
}

int
caen_v1290_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1290Module *v1290;

	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	caen_v1n90_init_slow(a_crate, &v1290->v1n90);
	return 1;
}

void
caen_v1290_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v1290_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct CaenV1290Module *v1290;

	(void)a_crate;
	(void)a_do_pedestals;
	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	return caen_v1n90_parse_data(&v1290->v1n90, a_event_buffer, 0);
}

uint32_t
caen_v1290_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV1290Module *v1290;

	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	return caen_v1n90_readout(a_crate, &v1290->v1n90, a_event_buffer);
}

uint32_t
caen_v1290_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1290Module *v1290;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	return caen_v1n90_readout_dt(&v1290->v1n90);
}

int
caen_v1290_register_list_pack(struct Module *a_module, struct PackerList
    *a_list)
{
	struct CaenV1290Module *v1290;

	MODULE_CAST(KW_CAEN_V1290, v1290, a_module);
	return caen_v1n90_register_list_pack(&v1290->v1n90, a_list, 8);
}

void
caen_v1290_setup_(void)
{
	MODULE_SETUP(caen_v1290, 0);
	MODULE_CALLBACK_BIND(caen_v1290, register_list_pack);
}
