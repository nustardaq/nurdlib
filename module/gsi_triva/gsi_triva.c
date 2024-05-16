/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
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

#include <module/gsi_triva/gsi_triva.h>
#include <module/gsi_triva/internal.h>
#include <module/gsi_triva/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>

#define NAME "Gsi Triva"

static void	cvt_set(struct Module *, unsigned);

MODULE_PROTOTYPES(gsi_triva);

void
cvt_set(struct Module *a_module, unsigned a_cvt_ns)
{
	struct GsiTrivaModule *triva;
	unsigned cvt;

	cvt = (a_cvt_ns + 99) / 100;
	cvt = 0xffff - MIN(cvt, 0xfffe);
	LOGF(verbose)(LOGL, NAME" cvt_set(%u ns = 0x%x) {", a_cvt_ns, cvt);
	MODULE_CAST(KW_GSI_TRIVA, triva, a_module);
	MAP_WRITE(triva->sicy_map, ctime, cvt);
	LOGF(verbose)(LOGL, NAME" cvt_set }");
}

uint32_t
gsi_triva_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
gsi_triva_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiTrivaModule *triva;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(triva);
	triva->address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address=%08x.", triva->address);
	triva->acvt_has = config_get_boolean(a_block, KW_ACVT);
	if (triva->acvt_has) {
		crate_acvt_set(a_crate, &triva->module, cvt_set);
	}
	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)triva;
}

void
gsi_triva_deinit(struct Module *a_module)
{
	struct GsiTrivaModule *triva;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_GSI_TRIVA, triva, a_module);
	map_unmap(&triva->sicy_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
gsi_triva_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
gsi_triva_get_map(struct Module *a_module)
{
	struct GsiTrivaModule *triva;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_GSI_TRIVA, triva, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return triva->sicy_map;
}

void
gsi_triva_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
gsi_triva_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	/* TODO: We actually have a few settings to play with. */
	(void)a_crate;
	(void)a_module;
	return 1;
}

int
gsi_triva_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiTrivaModule *triva;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_GSI_TRIVA, triva, a_module);
	triva->sicy_map = map_map(triva->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(ctime), MAP_POKE_REG(ctime), 50);
	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
gsi_triva_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_triva_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	log_die(LOGL, NAME" parse_data should not run!");
}

uint32_t
gsi_triva_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	log_die(LOGL, NAME" readout should not run!");
}

uint32_t
gsi_triva_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	log_die(LOGL, NAME" readout_dt should not run!");
}

void
gsi_triva_setup_(void)
{
	MODULE_SETUP(gsi_triva, MODULE_FLAG_EARLY_DT);
}
