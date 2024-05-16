/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2019-2020, 2023-2024
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

#include <module/gsi_pexaria/gsi_pexaria.h>
#include <module/gsi_pexaria/internal.h>
#include <nurdlib/crate.h>

#define NAME "Gsi Pexaria"
#define NO_DATA_TIMEOUT 1.0

MODULE_PROTOTYPES(gsi_pexaria);

uint32_t
gsi_pexaria_check_empty(struct Module *a_module)
{
	struct GsiPexariaModule *pexaria;

	MODULE_CAST(KW_GSI_PEXARIA, pexaria, a_module);
	return gsi_etherbone_check_empty(&pexaria->etherbone);
}

struct Module *
gsi_pexaria_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiPexariaModule *pexaria;

	(void)a_crate;
	MODULE_CREATE(pexaria);
	/* TODO: FIFO size? */
	pexaria->etherbone.module.event_max = 1;
	gsi_etherbone_create(a_block, &pexaria->etherbone, KW_GSI_PEXARIA);
	return (void *)pexaria;
}

void
gsi_pexaria_deinit(struct Module *a_module)
{
	struct GsiPexariaModule *pexaria;

	MODULE_CAST(KW_GSI_PEXARIA, pexaria, a_module);
	gsi_etherbone_deinit(&pexaria->etherbone);
}

void
gsi_pexaria_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
gsi_pexaria_get_map(struct Module *a_module)
{
	(void)a_module;
	return NULL;
}

void
gsi_pexaria_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
gsi_pexaria_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	return 1;
}

int
gsi_pexaria_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiPexariaModule *pexaria;

	(void)a_crate;
	MODULE_CAST(KW_GSI_PEXARIA, pexaria, a_module);
	return gsi_etherbone_init_slow(&pexaria->etherbone);
}

void
gsi_pexaria_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_pexaria_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

uint32_t
gsi_pexaria_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiPexariaModule *pexaria;

	(void)a_crate;
	MODULE_CAST(KW_GSI_PEXARIA, pexaria, a_module);
	return gsi_etherbone_readout(&pexaria->etherbone, a_event_buffer);
}

uint32_t
gsi_pexaria_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiPexariaModule *pexaria;

	(void)a_crate;
	MODULE_CAST(KW_GSI_PEXARIA, pexaria, a_module);
	return gsi_etherbone_readout_dt(&pexaria->etherbone);
}

void
gsi_pexaria_setup_(void)
{
	MODULE_SETUP(gsi_pexaria, MODULE_FLAG_EARLY_DT);
}
