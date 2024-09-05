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

#include <module/mesytec_mqdc32/mesytec_mqdc32.h>
#include <module/map/map.h>
#include <module/mesytec_mqdc32/internal.h>
#include <module/mesytec_mqdc32/offsets.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <util/bits.h>
#include <util/string.h>

#define NAME "Mesytec Mqdc32"

MODULE_PROTOTYPES(mesytec_mqdc32);
static int	mesytec_mqdc32_post_init(struct Crate *, struct Module *);
static void	mesytec_mqdc32_use_pedestals(struct Module *);
static void	mesytec_mqdc32_zero_suppress(struct Module *, int);

uint32_t
mesytec_mqdc32_check_empty(struct Module *a_module)
{

	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	return mesytec_mxdc32_check_empty(&mqdc32->mxdc32);
}

struct Module *
mesytec_mqdc32_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct MesytecMqdc32Module *mqdc32;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(mqdc32);
	/* Page 11 & 24 in the MQDC32 data sheet. */
	mqdc32->mxdc32.module.event_max = 65464 / 36;
	mesytec_mxdc32_create(a_block, &mqdc32->mxdc32);
	MODULE_CREATE_PEDESTALS(mqdc32, mesytec_mqdc32, 32);

	mqdc32->do_auto_pedestals = config_get_boolean(a_block,
	    KW_AUTO_PEDESTALS);
	LOGF(verbose)(LOGL, "Auto pedestals=%s.", mqdc32->do_auto_pedestals ?
	    "yes" : "no");

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)mqdc32;
}

void
mesytec_mqdc32_deinit(struct Module *a_module)
{
	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	mesytec_mxdc32_deinit(&mqdc32->mxdc32);
}

void
mesytec_mqdc32_destroy(struct Module *a_module)
{
	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	mesytec_mxdc32_destroy(&mqdc32->mxdc32);
}

struct Map *
mesytec_mqdc32_get_map(struct Module *a_module)
{
	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	return mesytec_mxdc32_get_map(&mqdc32->mxdc32);
}

void
mesytec_mqdc32_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(16, 23),
		BITS_MASK(24, 31) | BITS_MASK(12, 14),
		0x2 << 30)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
mesytec_mqdc32_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	enum Keyword const c_gate_input[] = {KW_ECL, KW_NIM};
	enum Keyword const c_exp_trig[] = {KW_OFF, KW_ECL, KW_NIM};
	uint16_t threshold_array[32];
	struct MesytecMqdc32Module *mqdc32;
	enum Keyword gate_input, exp_trig;
	double exp_t0, exp_t1;

	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);

	mesytec_mxdc32_init_fast(a_crate, &mqdc32->mxdc32,
	    MESYTEC_BANK_OP_CONNECTED | MESYTEC_BANK_OP_INDEPENDENT);

	mqdc32->mxdc32.channel_mask = config_get_bitmask(a_module->config,
	    KW_CHANNEL_ENABLE, 0, 31);

	CONFIG_GET_INT_ARRAY(threshold_array, a_module->config, KW_THRESHOLD,
	    CONFIG_UNIT_NONE, 0, 8191);
	MESYTEC_MXDC32_SET_THRESHOLDS(mqdc32, threshold_array);

	gate_input = CONFIG_GET_KEYWORD(a_module->config, KW_GATE_INPUT,
	    c_gate_input);
	LOGF(verbose)(LOGL, "Gate input=%s.", keyword_get_string(gate_input));
	MAP_WRITE(mqdc32->mxdc32.sicy_map, gate_select,
	    KW_NIM == gate_input ? 0 : 1);
	MAP_WRITE(mqdc32->mxdc32.sicy_map, nim_gat1_osc,
	    KW_NIM == gate_input ? 0 : 1);

	exp_trig = CONFIG_GET_KEYWORD(a_module->config, KW_EXP_TRIG,
	    c_exp_trig);
	exp_t0 = config_get_double(a_module->config, KW_EXP_TRIG_DELAY0,
	    CONFIG_UNIT_NS, 0, 16384);
	exp_t1 = config_get_double(a_module->config, KW_EXP_TRIG_DELAY1,
	    CONFIG_UNIT_NS, 0, 16384);
	LOGF(verbose)(LOGL, "Exp trig=%s.", keyword_get_string(exp_trig));
	if (KW_ECL == exp_trig) {
		MAP_WRITE(mqdc32->mxdc32.sicy_map, ecl_fc_reset, 2);
	} else if (KW_NIM == exp_trig) {
		MAP_WRITE(mqdc32->mxdc32.sicy_map, nim_fc_reset, 2);
	}
	if (KW_OFF != exp_trig) {
		LOGF(verbose)(LOGL, "Exp trig delay 0/1=%f/%f.", exp_t0, exp_t1);
		MAP_WRITE(mqdc32->mxdc32.sicy_map, exp_trig_delay0, exp_t0);
		MAP_WRITE(mqdc32->mxdc32.sicy_map, exp_trig_delay1, exp_t1);
	}

	/*
	 * The MQDC will by default write empty events.
	 * The requires that the multiplicity is a least 1.
	 */
	MAP_WRITE(mqdc32->mxdc32.sicy_map, low_limit0, 1);
	MAP_WRITE(mqdc32->mxdc32.sicy_map, low_limit1, 1);

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_mqdc32_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMqdc32Module *mqdc32;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	mesytec_mxdc32_init_slow(&mqdc32->mxdc32);

	return 1;
}

void
mesytec_mqdc32_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_mqdc32_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	return mesytec_mxdc32_parse_data(a_crate, &mqdc32->mxdc32,
	    a_event_buffer, a_do_pedestals && mqdc32->do_auto_pedestals,
	    1, 0);
}

int
mesytec_mqdc32_post_init(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMqdc32Module *mqdc32;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	return mesytec_mxdc32_post_init(&mqdc32->mxdc32);
}

uint32_t
mesytec_mqdc32_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	return mesytec_mxdc32_readout(a_crate, &mqdc32->mxdc32,
	    a_event_buffer, 1, 0);
}

uint32_t
mesytec_mqdc32_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMqdc32Module *mqdc32;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	return mesytec_mxdc32_readout_dt(&mqdc32->mxdc32);
}

void
mesytec_mqdc32_setup_(void)
{
	MODULE_SETUP(mesytec_mqdc32, 0);
	MODULE_CALLBACK_BIND(mesytec_mqdc32, post_init);
	MODULE_CALLBACK_BIND(mesytec_mqdc32, use_pedestals);
	MODULE_CALLBACK_BIND(mesytec_mqdc32, zero_suppress);
}

void
mesytec_mqdc32_use_pedestals(struct Module *a_module)
{
	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	MESYTEC_MXDC32_USE_PEDESTALS(mqdc32);
}

void
mesytec_mqdc32_zero_suppress(struct Module *a_module, int a_yes)
{
	struct MesytecMqdc32Module *mqdc32;

	MODULE_CAST(KW_MESYTEC_MQDC32, mqdc32, a_module);
	mesytec_mxdc32_zero_suppress(&mqdc32->mxdc32, a_yes);
}
