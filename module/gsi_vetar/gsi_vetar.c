/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
 * Bastian Löher
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

#include <module/gsi_vetar/gsi_vetar.h>
#include <module/gsi_vetar/internal.h>
#include <nurdlib/crate.h>

#define NAME "Gsi Vetar"
#define NO_DATA_TIMEOUT 1.0

MODULE_PROTOTYPES(gsi_vetar);

uint32_t
gsi_vetar_check_empty(struct Module *a_module)
{
	(void)a_module;
	/* TODO: Check FIFO counter? */
	return 0;
}

struct Module *
gsi_vetar_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiVetarModule *vetar;

	(void)a_crate;
	MODULE_CREATE(vetar);
	/* TODO: FIFO size? */
	vetar->etherbone.module.event_max = 30;
	gsi_etherbone_create(a_block, &vetar->etherbone, KW_GSI_VETAR);
	return (void *)vetar;
}

void
gsi_vetar_deinit(struct Module *a_module)
{
	struct GsiVetarModule *vetar;

	MODULE_CAST(KW_GSI_VETAR, vetar, a_module);
	gsi_etherbone_deinit(&vetar->etherbone);
}

void
gsi_vetar_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
gsi_vetar_get_map(struct Module *a_module)
{
	(void)a_module;
	return NULL;
}

void
gsi_vetar_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
gsi_vetar_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	return 1;
}

int
gsi_vetar_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiVetarModule *vetar;

	(void)a_crate;
	MODULE_CAST(KW_GSI_VETAR, vetar, a_module);
	return gsi_etherbone_init_slow(&vetar->etherbone);
}

void
gsi_vetar_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_vetar_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

uint32_t
gsi_vetar_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiVetarModule *vetar;

	(void)a_crate;
	MODULE_CAST(KW_GSI_VETAR, vetar, a_module);
	return gsi_etherbone_readout(&vetar->etherbone, a_event_buffer);
}

uint32_t
gsi_vetar_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiVetarModule *vetar;

	(void)a_crate;
	MODULE_CAST(KW_GSI_VETAR, vetar, a_module);
	return gsi_etherbone_readout_dt(&vetar->etherbone);
}

void
gsi_vetar_setup_(void)
{
	MODULE_SETUP(gsi_vetar, 0);
}
