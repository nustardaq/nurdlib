/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2024
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

#include <module/sis_3820/sis_3820.h>
#include <stdio.h>
#include <string.h>
#include <module/map/map.h>
#include <module/sis_3820/internal.h>
#include <module/sis_3820/offsets.h>
#include <nurdlib/crate.h>
#include <nurdlib/serialio.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>

#define N_CHANNELS 16

#define NAME "sis3820"
/* TODO: More than meets the eye... Check and extend this. */
#define REQUIRED_FIRMWARE 0x3820E301
#define DEFAULT_CLOCK_FREQ_KHZ 100000

MODULE_PROTOTYPES(sis_3820);

#define CHECK_REG_SET(reg, val) \
	{\
		SERIALIZE_IO;\
		if ((val) != (reg)) {\
			log_die(LOGL, NAME" register " #reg " value does "\
			    "not match %d != %d", (val), (reg));\
		}\
	}\

#define CHECK_REG_SET_MASK(reg, val, mask) \
	{\
		SERIALIZE_IO;\
		if ((val) != ((reg) & (mask))) {\
			log_die(LOGL, NAME" register " #reg " value does "\
			    "not match %08x != %08x & %08x",\
			    (val), (reg), (mask));\
		}\
	}\

uint32_t
sis_3820_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
sis_3820_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct Sis3820Module *m;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(m);
	m->module.event_max = 1e6;
	m->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address = %08x", m->address);

	LOGF(verbose)(LOGL, NAME" create }");
	return (void *)m;
}

void
sis_3820_deinit(struct Module *a_module)
{
	struct Sis3820Module *m;

	LOGF(info)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_SIS_3820, m, a_module);
	map_unmap(&m->sicy_map);
	LOGF(info)(LOGL, NAME" deinit }");
}

void
sis_3820_destroy(struct Module *a_module)
{
	LOGF(verbose)(LOGL, NAME" destroy {");
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
sis_3820_get_map(struct Module *a_module)
{
	struct Sis3820Module *m;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_SIS_3820, m, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return m->sicy_map;
}

void
sis_3820_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
sis_3820_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3820Module *m;
	uint32_t clock_freq_kHz;
	uint32_t clock_div_mode;

	(void)a_crate;

	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_SIS_3820, m, a_module);

	MAP_WRITE(m->sicy_map, reset, 1);

	/* Default clock source is second internal 100 MHz clock (U580). */
	clock_freq_kHz = config_get_int32(a_module->config, KW_CLK_FREQ,
	    CONFIG_UNIT_KHZ, 12000, 50000);
	LOGF(verbose)(LOGL, "Clock frequency = %d kHz.", clock_freq_kHz);
	switch (clock_freq_kHz) {
	case 50000:
		clock_div_mode = 0;
		break;
	case 25000:
		clock_div_mode = 1;
		break;
	case 12000:
		clock_div_mode = 2;
		break;
	default:
		log_die(LOGL, NAME"Unsupported clock frequency: %d",
		    clock_freq_kHz);
	}

	/* Set clock source and divider. */
	MAP_WRITE(m->sicy_map, clock_source,
	    (1 << 0) /* Use second internal clock. */
	    | (clock_div_mode << 4));
	LOGF(info)(LOGL, "clock_div_mode = %u.", clock_div_mode);

	/* Set clock frequency. */
	if (clock_div_mode == 3) {
		uint32_t clock_frequency_n;

		clock_frequency_n = (DEFAULT_CLOCK_FREQ_KHZ /
		    (clock_freq_kHz * 4)) - 1;
		MAP_WRITE(m->sicy_map, clock_frequency, clock_frequency_n);
		LOGF(info)(LOGL, "clock_frequency_n = %u.",
		    clock_frequency_n);
	}

	MAP_WRITE(m->sicy_map, control_status, 1 << 15); /* General enable. */

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
sis_3820_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3820Module *m;
	uint32_t firmware;

	(void)a_crate;

	LOGF(info)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_SIS_3820, m, a_module);

	/* Map single cycle. */
	m->sicy_map = map_map(m->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(clock_frequency), MAP_POKE_REG(clock_frequency), 0);

	/* Check firmware. */
	firmware = MAP_READ(m->sicy_map, module_id_firmware);
	if (firmware < REQUIRED_FIRMWARE) {
		log_die(LOGL, NAME"Actual firmware=%08x < required firmware="
		    "%08x.", firmware, REQUIRED_FIRMWARE);
	}
	LOGF(info)(LOGL, "id+firmware=0x%08x.", firmware);

	LOGF(info)(LOGL, NAME" init_slow }");
	return 1;
}

void
sis_3820_memtest(struct Module *a_module, enum Keyword a_memtest_mode)
{
	(void)a_module;
	(void)a_memtest_mode;
}

uint32_t
sis_3820_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

uint32_t
sis_3820_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	return 0;
}

uint32_t
sis_3820_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	log_die(LOGL, NAME" readout_dt not implemented!");
}

void
sis_3820_setup_(void)
{
	MODULE_SETUP(sis_3820, 0);
}
