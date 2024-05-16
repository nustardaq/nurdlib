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

#include <module/caen_v895/caen_v895.h>
#include <module/caen_v895/internal.h>
#include <module/caen_v895/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/math.h>
#include <util/string.h>

#define NAME "Caen v895"
#define OW_A 7.28571428571429
#define OW_B -36.42857142857143

MODULE_PROTOTYPES(caen_v895);

uint32_t
caen_v895_check_empty(struct Module *a_module)
{
	(void)a_module;
	log_die(LOGL, NAME" check_empty should not be run!");
}

struct Module *
caen_v895_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV895Module *v895;

	LOGF(verbose)(LOGL, NAME" create {");

	(void)a_crate;
	MODULE_CREATE(v895);
	v895->address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address=%08x.", v895->address);

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)v895;
}

void
caen_v895_deinit(struct Module *a_module)
{
	struct CaenV895Module *v895;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_CAEN_V895, v895, a_module);
	map_unmap(&v895->sicy_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
caen_v895_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
caen_v895_get_map(struct Module *a_module)
{
	struct CaenV895Module *v895;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_CAEN_V895, v895, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return v895->sicy_map;
}

void
caen_v895_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
caen_v895_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	char str[128];
	uint8_t	threshold_array[16];
	struct CaenV895Module *v895;
	uint32_t channel_mask;
	unsigned output_width_0_7;
	unsigned output_width_8_15;
	int i, ow, ofs;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_CAEN_V895, v895, a_module);

	CONFIG_GET_INT_ARRAY(threshold_array, a_module->config, KW_THRESHOLD,
	    CONFIG_UNIT_MV, 0, 255);
	ofs = snprintf(str, sizeof str, "Thresholds=(");
	for (i = 0; 16 > i; ++i) {
		MAP_WRITE(v895->sicy_map, threshold(i), threshold_array[i]);
		ofs += snprintf(str + ofs, sizeof str - ofs, " %d",
		    threshold_array[i]);
	}
	LOGF(verbose)(LOGL, "%s).", str);

	output_width_0_7 = config_get_double(a_module->config,
	    KW_OUTPUT_WIDTH_0_7, CONFIG_UNIT_NS, 5, 40);
	output_width_8_15 = config_get_double(a_module->config,
	    KW_OUTPUT_WIDTH_8_15, CONFIG_UNIT_NS, 5, 40);
	ofs = snprintf(str, sizeof str, "Widths=(");
	ow = OW_A * output_width_0_7 + OW_B;
	ow = CLAMP(ow, 0, 255);
	MAP_WRITE(v895->sicy_map, output_width_0_to_7, ow);
	ofs += snprintf(str + ofs, sizeof str - ofs, " %d", ow);
	ow = OW_A * output_width_8_15 + OW_B;
	ow = CLAMP(ow, 0, 255);
	MAP_WRITE(v895->sicy_map, output_width_8_to_15, ow);
	ofs += snprintf(str + ofs, sizeof str - ofs, " %d)", ow);
	LOGF(verbose)(LOGL, "%s.", str);

	channel_mask = config_get_bitmask(a_module->config, KW_CHANNEL_ENABLE,
	    0, 15);
	MAP_WRITE(v895->sicy_map, pattern_inhibit, channel_mask);

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v895_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV895Module *v895;
	uint16_t id;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_CAEN_V895, v895, a_module);

	v895->sicy_map = map_map(v895->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(fixed_code), MAP_POKE_REG(test_pulse), 0);

	id = MAP_READ(v895->sicy_map, fixed_code);
	if (0xfaf5 != id) {
		log_die(LOGL, "Fixed code=0x%04x != 0xfaf5, module borked?",
		    id);
	}
	id = MAP_READ(v895->sicy_map, manufacturer_module_type);
	LOGF(verbose)(LOGL, "Manufacturer=%02x, module type=%03x (0x%04x).",
	    (0xfc00 & id) >> 10, 0x3ff & id, id);
	id = MAP_READ(v895->sicy_map, version_serial_number);
	LOGF(verbose)(LOGL, "Version=%x, S/N=%03x (0x%04x).",
	    (0xf000 & id) >> 12, 0xfff & id, id);

	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
caen_v895_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v895_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	log_die(LOGL, NAME" parse_data should not be run!");
}

uint32_t
caen_v895_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	log_die(LOGL, NAME" readout should not be run!");
}

uint32_t
caen_v895_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	log_die(LOGL, NAME" readout_dt should not run!");
}

void
caen_v895_setup_(void)
{
	MODULE_SETUP(caen_v895, 0);
}
