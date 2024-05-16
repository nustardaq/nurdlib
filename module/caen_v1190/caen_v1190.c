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

#include <module/caen_v1190/caen_v1190.h>
#include <module/caen_v1190/internal.h>
#include <module/map/map.h>
#include <nurdlib/log.h>

#define NAME "Caen v1190"

MODULE_PROTOTYPES(caen_v1190);

static void	caen_v1190_register_list_pack(struct Module *, struct
    PackerList *);

uint32_t
caen_v1190_check_empty(struct Module *a_module)
{
	struct CaenV1190Module *v1190;

	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	return caen_v1n90_check_empty(&v1190->v1n90);
}

struct Module *
caen_v1190_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV1190Module *v1190;

	(void)a_crate;
	MODULE_CREATE(v1190);
	caen_v1n90_create(a_block, &v1190->v1n90);
	return &v1190->v1n90.module;
}

void
caen_v1190_deinit(struct Module *a_module)
{
	struct CaenV1190Module *v1190;

	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	caen_v1n90_deinit(&v1190->v1n90);
}

void
caen_v1190_destroy(struct Module *a_module)
{
	struct CaenV1190Module *v1190;

	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	caen_v1n90_destroy(&v1190->v1n90);
}

struct Map *
caen_v1190_get_map(struct Module *a_module)
{
	struct CaenV1190Module *v1190;

	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	return caen_v1n90_get_map(&v1190->v1n90);
}

void
caen_v1190_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	caen_v1n90_get_signature(a_array, a_num);
}

int
caen_v1190_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1190Module *v1190;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	caen_v1n90_init_fast(&v1190->v1n90, KW_CAEN_V1190);
	return 1;
}

int
caen_v1190_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1190Module *v1190;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	caen_v1n90_init_slow(&v1190->v1n90);
	return 1;
}

void
caen_v1190_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v1190_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct CaenV1190Module *v1190;

	(void)a_crate;
	(void)a_do_pedestals;
	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	return caen_v1n90_parse_data(&v1190->v1n90, a_event_buffer, 0);
}

uint32_t
caen_v1190_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV1190Module *v1190;

	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	return caen_v1n90_readout(a_crate, &v1190->v1n90, a_event_buffer);
}

uint32_t
caen_v1190_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1190Module *v1190;

	(void)a_crate;
	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	return caen_v1n90_readout_dt(&v1190->v1n90);
}

void
caen_v1190_register_list_pack(struct Module *a_module, struct PackerList
    *a_list)
{
	struct CaenV1190Module *v1190;

	MODULE_CAST(KW_CAEN_V1190, v1190, a_module);
	caen_v1n90_register_list_pack(&v1190->v1n90, a_list, 4);
}

void
caen_v1190_setup_(void)
{
	MODULE_SETUP(caen_v1190, 0);
	MODULE_CALLBACK_BIND(caen_v1190, register_list_pack);
}
