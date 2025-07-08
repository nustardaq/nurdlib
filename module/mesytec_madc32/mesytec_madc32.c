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

#include <module/mesytec_madc32/mesytec_madc32.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_madc32/internal.h>
#include <module/mesytec_madc32/offsets.h>
#include <nurdlib/config.h>
#include <util/bits.h>
#include <util/string.h>

#define NAME "Mesytec Madc32"

MODULE_PROTOTYPES(mesytec_madc32);
static int	mesytec_madc32_post_init(struct Crate *, struct Module *)
	FUNC_RETURNS;
static uint32_t	mesytec_madc32_readout_shadow(struct Crate *, struct Module *,
    struct EventBuffer *) FUNC_RETURNS;
static void	mesytec_madc32_use_pedestals(struct Module *);
static void	mesytec_madc32_zero_suppress(struct Module *, int);

uint32_t
mesytec_madc32_check_empty(struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	return mesytec_mxdc32_check_empty(&madc32->mxdc32);
}

struct Module *
mesytec_madc32_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct MesytecMadc32Module *madc32;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(madc32);
	mesytec_mxdc32_create(a_block, &madc32->mxdc32);
	MODULE_CREATE_PEDESTALS(madc32, mesytec_madc32, 32);

	madc32->do_auto_pedestals = config_get_boolean(a_block,
	    KW_AUTO_PEDESTALS);
	LOGF(verbose)(LOGL, "Auto pedestals=%s.", madc32->do_auto_pedestals ?
	    "yes" : "no");

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)madc32;
}

void
mesytec_madc32_deinit(struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	mesytec_mxdc32_deinit(&madc32-> mxdc32);
}

void
mesytec_madc32_destroy(struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	mesytec_mxdc32_destroy(&madc32-> mxdc32);
}

struct Map *
mesytec_madc32_get_map(struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	return mesytec_mxdc32_get_map(&madc32->mxdc32);
}

void
mesytec_madc32_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(16, 23),
		BITS_MASK(24, 31) | BITS_MASK(10, 11),
		0x2 << 30)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
mesytec_madc32_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	enum Keyword const c_nim_busy[] = {KW_BUSY, KW_GATE0};
	struct ModuleGate gate;
	struct MesytecMadc32Module *madc32;
	uint16_t threshold_array[32];
	uint16_t fw;
	size_t i;
	unsigned resolution, resolution_hires;
	unsigned gate_offset, gate_width;
	unsigned range_v;
	enum Keyword nim_busy;

	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);

	mesytec_mxdc32_init_fast(a_crate, &madc32->mxdc32,
	    MESYTEC_BANK_OP_CONNECTED | MESYTEC_BANK_OP_INDEPENDENT |
	    MESYTEC_BANK_OP_TOGGLE);

	fw = MAP_READ(madc32->mxdc32.sicy_map, firmware_revision);

	madc32->mxdc32.channel_mask = config_get_bitmask(a_module->config,
	    KW_CHANNEL_ENABLE, 0, 31);

	CONFIG_GET_INT_ARRAY(threshold_array, a_module->config, KW_THRESHOLD,
	    CONFIG_UNIT_NONE, 0, 8191);
	MESYTEC_MXDC32_SET_THRESHOLDS(madc32, threshold_array);

	/* Resolution + hires. */
	resolution = config_get_int32(a_module->config, KW_RESOLUTION,
	    CONFIG_UNIT_NONE, 2, 8);
	resolution_hires = config_get_boolean(a_module->config, KW_HIRES);
	resolution = 4 < resolution ? 8 : 4;
	MAP_WRITE(madc32->mxdc32.sicy_map, adc_resolution,
	    (resolution / 2 - 1) + resolution_hires);

	/* Use software gate. */
	module_gate_get(&gate, a_module->config, 25.0, 12850.0, 0.0, 12750.0);
	if (25.0 >= -gate.time_after_trigger_ns) {
		gate_offset = 0;
	} else if (150.0 >= -gate.time_after_trigger_ns) {
		gate_offset = 1;
	} else {
		gate_offset = 1 +
		    ceil((-gate.time_after_trigger_ns - 150.0) / 50.0);
	}
	gate_width = ceil(gate.width_ns / 50.0);
	LOGF(verbose)(LOGL, "Trigger window=(0x%04x,0x%04x).", gate_offset,
	    gate_width);
	MAP_WRITE(madc32->mxdc32.sicy_map, hold_delay0, gate_offset);
	MAP_WRITE(madc32->mxdc32.sicy_map, hold_width0, gate_width);

	/* Input range voltage. */
	range_v = config_get_int32(a_module->config, KW_RANGE, CONFIG_UNIT_V,
	    3, 11);
	if (8 < range_v) {
		range_v = 10;
	} else if (4 < range_v) {
		range_v = 8;
	} else {
		range_v = 4;
	}
	LOGF(verbose)(LOGL, "Input range=%u V", range_v);
	switch (range_v) {
	case 4: MAP_WRITE(madc32->mxdc32.sicy_map, input_range, 0); break;
	case 8: MAP_WRITE(madc32->mxdc32.sicy_map, input_range, 2); break;
	case 10: MAP_WRITE(madc32->mxdc32.sicy_map, input_range, 1); break;
	default: log_die(LOGL, "This can not happen!");
	}

	/* Show gate0 on busy out. */
	nim_busy = CONFIG_GET_KEYWORD(a_module->config, KW_NIM_BUSY,
	    c_nim_busy);
	for (i = 0; c_nim_busy[i] != nim_busy; ++i)
		;
	MAP_WRITE(madc32->mxdc32.sicy_map, nim_busy, i);

	/* Munch: By default the MADC is _only_ busy while it is
	   converting and _not_ when the buffer is full.
	   Buffer full busy is needed for shadow readout.

	   Using nim_busy = 5 (undocument added by Robert on
	   request from me) the BUSY is asserted when converting
	   or the number of buffer words is larger >= irq_threshold.
	   This busy mode was added in 0x132 and 0x227.

	   Below we set irq_threshold. However, it is set lower than
	   max since buffer overflow has been observed at high rates.
	 */
	if (fw >= 0x0200) {
		MAP_WRITE(madc32->mxdc32.sicy_map, irq_threshold, 8120-8*34);
	}
	else {
		MAP_WRITE(madc32->mxdc32.sicy_map, irq_threshold, 1024-8*34);
	}

	if (nim_busy == KW_BUSY &&
	    (fw >= 0x227 || (fw >= 0x132 && fw < 0x200))) {
		MAP_WRITE(madc32->mxdc32.sicy_map, nim_busy, 5);
		MAP_WRITE(madc32->mxdc32.sicy_map, ecl_busy, 5);
		LOGF(verbose)(LOGL, "nim_busy = 5 (conversion + buffer");
	} else {
		LOGF(verbose)(LOGL, "nim_busy %"PRIz".", i);
	}

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_madc32_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	mesytec_mxdc32_init_slow(&madc32->mxdc32);

	/* Use software gate. */
	MAP_WRITE(madc32->mxdc32.sicy_map, use_gg, 1);

	/* p. 1 & 15 + testing. */
	madc32->mxdc32.module.event_max =
	    (MAP_READ(madc32->mxdc32.sicy_map, firmware_revision)
	     < 0x200 ? 1000 : 8000) / 36;

	LOGF(info)(LOGL, NAME" init_slow }");
	return 1;
}

void
mesytec_madc32_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_madc32_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	return mesytec_mxdc32_parse_data(a_crate, &madc32->mxdc32,
	    a_event_buffer, a_do_pedestals && madc32->do_auto_pedestals,
	    0, 0);
}

int
mesytec_madc32_post_init(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	return mesytec_mxdc32_post_init(&madc32->mxdc32);
}

uint32_t
mesytec_madc32_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	return mesytec_mxdc32_readout(a_crate, &madc32->mxdc32,
	    a_event_buffer, 0, 0);
}

uint32_t
mesytec_madc32_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	return mesytec_mxdc32_readout_dt(a_crate, &madc32->mxdc32);
}

uint32_t
mesytec_madc32_readout_shadow(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	struct MesytecMadc32Module *madc32;
	uint32_t result;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	result = mesytec_mxdc32_readout_shadow(&madc32->mxdc32,
	    a_event_buffer, 1);
	return result;
}

void
mesytec_madc32_setup_(void)
{
	MODULE_SETUP(mesytec_madc32, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(mesytec_madc32, post_init);
	MODULE_CALLBACK_BIND(mesytec_madc32, readout_shadow);
	MODULE_CALLBACK_BIND(mesytec_madc32, use_pedestals);
	MODULE_CALLBACK_BIND(mesytec_madc32, zero_suppress);
}

void
mesytec_madc32_use_pedestals(struct Module *a_module)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	MESYTEC_MXDC32_USE_PEDESTALS(madc32);
}

void
mesytec_madc32_zero_suppress(struct Module *a_module, int a_yes)
{
	struct MesytecMadc32Module *madc32;

	MODULE_CAST(KW_MESYTEC_MADC32, madc32, a_module);
	mesytec_mxdc32_zero_suppress(&madc32->mxdc32, a_yes);
}
