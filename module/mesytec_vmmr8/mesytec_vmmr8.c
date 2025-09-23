/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2019-2024
 * Bastian Löher
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

#include <module/mesytec_vmmr8/mesytec_vmmr8.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_vmmr8/internal.h>
#include <module/mesytec_vmmr8/offsets.h>
#include <nurdlib/config.h>
#include <util/bits.h>
#include <util/time.h>

#define NAME "Mesytec VMMR8"

MODULE_PROTOTYPES(mesytec_vmmr8);
static int	mesytec_vmmr8_post_init(struct Crate *, struct Module *)
	FUNC_RETURNS;
static uint32_t	mesytec_vmmr8_readout_shadow(struct Crate *, struct Module *,
    struct EventBuffer *) FUNC_RETURNS;
#if NCONF_mMAP_bCMVLC
static void	mesytec_vmmr8_cmvlc_init(struct Module *,
    struct cmvlc_stackcmdbuf *, int);
static uint32_t mesytec_vmmr8_cmvlc_fetch_dt(struct Module *,
    const uint32_t *, uint32_t, uint32_t *);
static uint32_t mesytec_vmmr8_cmvlc_fetch(struct Crate *, struct
    Module *, struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *);
#endif

uint32_t
mesytec_vmmr8_check_empty(struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_check_empty(&vmmr8->mxdc32);
}

struct Module *
mesytec_vmmr8_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{

	struct ModuleGate gate;
	struct MesytecVmmr8Module *vmmr8;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(vmmr8);
	mesytec_mxdc32_create(a_block, &vmmr8->mxdc32);

	/* PULSER */
	vmmr8->config.pulser_enabled = config_get_boolean(a_block,
	    KW_PULSER_ENABLED);
	LOGF(verbose)(LOGL, "Pulser status=%s.", vmmr8->config.pulser_enabled
	    ? "on" : "off");
	vmmr8->config.pulser_amplitude = config_get_int32(a_block,
	    KW_PULSER_AMPLITUDE, CONFIG_UNIT_NONE, 0, 0xff);
	LOGF(verbose)(LOGL, "Pulser_amplitude=%d.",
	    vmmr8->config.pulser_amplitude);

	/* use_external_clock? if yes to be added in internal.h. */

	/* ACTIVE BUSES */
	CONFIG_GET_INT_ARRAY(vmmr8->config.active_buses, a_block,
	    KW_ACTIVE_BUSES, CONFIG_UNIT_NONE, 0, 1);

	/*
	 * TIMING_RESOLUTION
	 * Only available starting from firmware SW-RevX110 and higher.
	 */
	vmmr8->config.timing_resolution = config_get_int32(a_block,
	    KW_TIMING_RESOLUTION, CONFIG_UNIT_NONE, 0, 1);

	/* NIM I/O */
	CONFIG_GET_INT_ARRAY(vmmr8->config.nim, a_block, KW_NIM,
	    CONFIG_UNIT_NONE, 0, 9);
	if (0 != vmmr8->config.nim[0] &&
	    1 != vmmr8->config.nim[0] &&
	    4 != vmmr8->config.nim[0] &&
	    8 != vmmr8->config.nim[0] &&
	    9 != vmmr8->config.nim[0]) {
		log_die(LOGL, "NIM 0=%u invalid choice!",
		    vmmr8->config.nim[0]);
	}
	if (0 > vmmr8->config.nim[2] ||
	    2 < vmmr8->config.nim[2]) {
		log_die(LOGL, "NIM 2=%u invalid choice!",
		    vmmr8->config.nim[2]);
	}
	if (0 > vmmr8->config.nim[3] ||
	    2 < vmmr8->config.nim[3]) {
		log_die(LOGL, "NIM 3=%u invalid choice!",
		    vmmr8->config.nim[3]);
	}

	/* ECL I/O */
	CONFIG_GET_INT_ARRAY(vmmr8->config.ecl, a_block, KW_ECL,
	    CONFIG_UNIT_NONE, 0, 0xff);
	if (0 != vmmr8->config.ecl[0] &&
	    4 != vmmr8->config.ecl[0] &&
	    8 != vmmr8->config.ecl[0] &&
	    9 != vmmr8->config.ecl[0]) {
		log_die(LOGL, "ECL 0=%u invalid choice!",
		    vmmr8->config.ecl[0]);
	}
	if ((0 != (vmmr8->config.ecl[1] & 0xf) &&
	     1 != (vmmr8->config.ecl[1] & 0xf)) ||
	    (0 != (vmmr8->config.ecl[1] >> 4) &&
	     1 != (vmmr8->config.ecl[1] >> 4))) {
		log_die(LOGL, "ECL 0=%u invalid choice!",
		    vmmr8->config.ecl[1]);
	}
	if ((0 != (vmmr8->config.ecl[1] & 0xf) &&
	     1 != (vmmr8->config.ecl[1] & 0xf) &&
	     2 != (vmmr8->config.ecl[1] & 0xf)) ||
	    (0 != (vmmr8->config.ecl[1] >> 4) &&
	     1 != (vmmr8->config.ecl[1] >> 4))) {
		log_die(LOGL, "ECL 0=%u invalid choice!",
		    vmmr8->config.ecl[1]);
	}
	if ((0 != (vmmr8->config.ecl[3] & 0xf) &&
	     1 != (vmmr8->config.ecl[3] & 0xf)) ||
	    (0 != (vmmr8->config.ecl[3] >> 4) &&
	     1 != (vmmr8->config.ecl[3] >> 4))) {
		log_die(LOGL, "ECL 0=%u invalid choice!",
		    vmmr8->config.ecl[3]);
	}

	/* TRIGGERS : SIGNAL WHICH CREATES THE WINDOW OF INTEREST */
	vmmr8->config.use_ext_trg0 = config_get_boolean(a_block,
	    KW_USE_EXTERNAL_TRIGGER0);
	LOGF(verbose)(LOGL, "EXTERNAL TRIGGER 0=%s.",
	    vmmr8->config.use_ext_trg0 ? "on" : "off");
	vmmr8->config.use_ext_trg1 = config_get_boolean(a_block,
	    KW_USE_EXTERNAL_TRIGGER1);
	LOGF(verbose)(LOGL, "EXTERNAL TRIGGER 1=%s.",
	    vmmr8->config.use_ext_trg1 ? "on" : "off");
	vmmr8->config.use_int_trg = config_get_bitmask(a_block,
	    KW_TRIGGER_CHANNEL, 0, 0xff);
	LOGF(debug)(LOGL, "input channels 0x%04x use as INTERNAL TRIGGER.",
	    vmmr8->config.use_int_trg);
	/* TODO: THE CHECKING ON THE TRIGGER */

	/* TRIGGER OUT : SIGNAL SENT TO THE NIM1 OUTPUT */
	vmmr8->config.trigger_output_src = config_get_int32(a_block,
	    KW_TRIGGER_OUTPUT_SRC, CONFIG_UNIT_NONE, 0, 1);
	vmmr8->config.trigger_output = config_get_bitmask(a_block,
	    KW_TRIGGER_OUTPUT, 0, 0xff);

	/* SEARCHING WINDOW */
	module_gate_get(&gate, a_block, -20480.0, 20480.0, 0.0, 20480.0);
	vmmr8->config.gate_offset = ceil((gate.time_after_trigger_ns +
	    20480.0) / 5.0);
	vmmr8->config.gate_width = ceil(gate.width_ns / 5.0);
	LOGF(verbose)(LOGL, "Trigger window=(0x%04x,0x%04x).",
	    vmmr8->config.gate_offset, vmmr8->config.gate_width);

	/* THRESHOLDS OF BANK0 : 32 LOWER CHANNELS OF MMR64 */
	CONFIG_GET_INT_ARRAY(vmmr8->config.mmr64_thrs0, a_block,
	    KW_MMR64_THRS_BANK0, CONFIG_UNIT_NONE, 0, 255);

	/* THRESHOLDS OF BANK1 : 32 UPPER CHANNELS OF MMR64 */
	CONFIG_GET_INT_ARRAY(vmmr8->config.mmr64_thrs1, a_block,
	    KW_MMR64_THRS_BANK1, CONFIG_UNIT_NONE, 0, 255);

	/* ZERO SUPPRESSION */
	CONFIG_GET_INT_ARRAY(vmmr8->config.data_thrs, a_block,
	    KW_ZERO_SUPPRESS, CONFIG_UNIT_NONE, 0, 8191);

	/* TODO. */
	vmmr8->mxdc32.module.event_max = 100;

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)vmmr8;
}

void
mesytec_vmmr8_deinit(struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	mesytec_mxdc32_deinit(&vmmr8->mxdc32);
}

void
mesytec_vmmr8_destroy(struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	mesytec_mxdc32_destroy(&vmmr8->mxdc32);
}

struct Map *
mesytec_vmmr8_get_map(struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_get_map(&vmmr8->mxdc32);
}

void
mesytec_vmmr8_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(16, 23),
		BITS_MASK(25, 31) | BITS_MASK(12, 15),
		0x2 << 30)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature }");
}

int
mesytec_vmmr8_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;
	double init_sleep;
	unsigned i;
	uint16_t mask;

	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	mesytec_mxdc32_init_fast(a_crate, &vmmr8->mxdc32, 0);

	mask = 0;
	for (i = 0; i < 8; i++) {
		mask |= vmmr8->config.active_buses[i] << i;
	}
	MAP_WRITE(vmmr8->mxdc32.sicy_map, active_buses, mask);

	MAP_WRITE(vmmr8->mxdc32.sicy_map, timing_resolution,
	    vmmr8->config.timing_resolution);

	mask = 0;
	mask |= vmmr8->config.use_ext_trg0 << 0;
	mask |= vmmr8->config.use_ext_trg1 << 1;
	MAP_WRITE(vmmr8->mxdc32.sicy_map, ext_trig_source, mask);

	MAP_WRITE(vmmr8->mxdc32.sicy_map, trig_source,
	    vmmr8->config.use_int_trg);

	MAP_WRITE(vmmr8->mxdc32.sicy_map, win_start,
	    vmmr8->config.gate_offset);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, win_width,
	    vmmr8->config.gate_width);

	MAP_WRITE(vmmr8->mxdc32.sicy_map, nim3, vmmr8->config.nim[3]);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, nim2, vmmr8->config.nim[2]);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, nim0, vmmr8->config.nim[0]);

	MAP_WRITE(vmmr8->mxdc32.sicy_map, ecl3, vmmr8->config.ecl[3]);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, ecl2, vmmr8->config.ecl[2]);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, ecl1, vmmr8->config.ecl[1]);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, ecl0, vmmr8->config.ecl[0]);

	init_sleep = mesytec_mxdc32_sleep_get(&vmmr8->mxdc32);

	/* TODO: Reminaing settings are sensitive to timing? */
	time_sleep(init_sleep);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, pulser_status,
	    vmmr8->config.pulser_enabled);
	time_sleep(init_sleep);
	MAP_WRITE(vmmr8->mxdc32.sicy_map, pulser_amplitude,
	    vmmr8->config.pulser_amplitude);
	time_sleep(init_sleep);

	/* Threshold and zero suppression */
	for (i = 0; i < 8; i++) {
		MAP_WRITE(vmmr8->mxdc32.sicy_map, select_bus, i);
		time_sleep(init_sleep);
		/* 2a/ select the 32 lower channels of the MMR64 */
		MAP_WRITE(vmmr8->mxdc32.sicy_map, fe_addr_wr, 9);
		time_sleep(init_sleep);
		/* 2b/ write the bank0 threshold (com_thrs0) */
		MAP_WRITE(vmmr8->mxdc32.sicy_map, fe_data_wr,
		    vmmr8->config.mmr64_thrs0[i]);
		time_sleep(init_sleep);
		/* 3a/ select the 32 upper channels of the MMR64 */
		MAP_WRITE(vmmr8->mxdc32.sicy_map, fe_addr_wr, 10);
		time_sleep(init_sleep);
		/* 3b/ write the bank1 threshold (com_thrs1) */
		MAP_WRITE(vmmr8->mxdc32.sicy_map, fe_data_wr,
		    vmmr8->config.mmr64_thrs1[i]);
		time_sleep(init_sleep);
		/* 4a/ select the zero suppression (data_threshold)  */
		MAP_WRITE(vmmr8->mxdc32.sicy_map, fe_addr_wr, 16);
		time_sleep(init_sleep);
		/* 4b/ get value of threshold for zero suppr and write */
		MAP_WRITE(vmmr8->mxdc32.sicy_map, fe_data_wr,
		    vmmr8->config.data_thrs[i]);
		time_sleep(init_sleep);
	}

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_vmmr8_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	mesytec_mxdc32_init_slow(&vmmr8->mxdc32);

	return 1;
}

void
mesytec_vmmr8_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_vmmr8_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecVmmr8Module *vmmr8;

	(void)a_do_pedestals;
	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_parse_data(a_crate, &vmmr8->mxdc32,
	    a_event_buffer, 0, 1, 0);
}

int
mesytec_vmmr8_post_init(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_post_init(&vmmr8->mxdc32);
}

uint32_t
mesytec_vmmr8_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_readout(a_crate, &vmmr8->mxdc32, a_event_buffer,
	    1, 0);
}

uint32_t
mesytec_vmmr8_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_readout_dt(a_crate, &vmmr8->mxdc32);
}

uint32_t
mesytec_vmmr8_readout_shadow(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	struct MesytecVmmr8Module *vmmr8;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_readout_shadow(&vmmr8->mxdc32, a_event_buffer,
	    0);
}

#if NCONF_mMAP_bCMVLC
void
mesytec_vmmr8_cmvlc_init(struct Module *a_module,
    struct cmvlc_stackcmdbuf *a_stack, int a_dt)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	mesytec_mxdc32_cmvlc_init(&vmmr8->mxdc32, a_stack, a_dt);
}

uint32_t
mesytec_vmmr8_cmvlc_fetch_dt(struct Module *a_module,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_cmvlc_fetch_dt(&vmmr8->mxdc32,
	    a_in_buffer, a_in_remain, a_in_used);
}

uint32_t
mesytec_vmmr8_cmvlc_fetch(struct Crate *a_crate,
    struct Module *a_module, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecVmmr8Module *vmmr8;

	MODULE_CAST(KW_MESYTEC_VMMR8, vmmr8, a_module);
	return mesytec_mxdc32_cmvlc_fetch(a_crate, &vmmr8->mxdc32,
	    a_event_buffer, a_in_buffer, a_in_remain, a_in_used);
}
#endif

void
mesytec_vmmr8_setup_(void)
{
	MODULE_SETUP(mesytec_vmmr8, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(mesytec_vmmr8, post_init);
	MODULE_CALLBACK_BIND(mesytec_vmmr8, readout_shadow);
#if NCONF_mMAP_bCMVLC
	MODULE_CALLBACK_BIND(mesytec_vmmr8, cmvlc_init);
	MODULE_CALLBACK_BIND(mesytec_vmmr8, cmvlc_fetch_dt);
	MODULE_CALLBACK_BIND(mesytec_vmmr8, cmvlc_fetch);
#endif
}
