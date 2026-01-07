/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
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

#include <module/mesytec_mdpp/internal.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_mdpp/offsets.h>
#include <nurdlib/config.h>
#include <util/bits.h>
#include <util/time.h>

#define NAME "Mesytec Mdpp"

static void thresholds_set(struct MesytecMdppModule *, uint16_t const *);

void
mesytec_mdpp_create_(struct ConfigBlock *a_block, struct MesytecMdppModule
    *a_mdpp, enum Keyword a_type)
{
	LOGF(verbose)(LOGL, NAME" create {");

	if (KW_MESYTEC_MDPP16SCP == a_type ||
	    KW_MESYTEC_MDPP16QDC == a_type) {
		a_mdpp->config.ch_num = 16;
	} else if (KW_MESYTEC_MDPP32SCP == a_type ||
	    KW_MESYTEC_MDPP32QDC == a_type) {
		a_mdpp->config.ch_num = 32;
	} else {
		log_die(LOGL, NAME" Module %s should not derive from me!",
		    keyword_get_string(a_type));
	}

	mesytec_mxdc32_create(a_block, &a_mdpp->mxdc32);

	a_mdpp->config.do_auto_pedestals = config_get_boolean(
	    a_block, KW_AUTO_PEDESTALS);
	LOGF(verbose)(LOGL, "Auto pedestals=%s.",
	    a_mdpp->config.do_auto_pedestals ? "yes" : "no");

	LOGF(verbose)(LOGL, NAME" create }");
}

void
mesytec_mdpp_deinit(struct MesytecMdppModule *a_mdpp)
{
	mesytec_mxdc32_deinit(&a_mdpp->mxdc32);
}

void
mesytec_mdpp_destroy(struct MesytecMdppModule *a_mdpp)
{
	mesytec_mxdc32_destroy(&a_mdpp->mxdc32);
}

struct Map *
mesytec_mdpp_get_map(struct MesytecMdppModule *a_mdpp)
{
	return mesytec_mxdc32_get_map(&a_mdpp->mxdc32);
}

void
mesytec_mdpp_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	/*
	 * TODO: If free-running:
	 *
	 * a_sig->id_mask = BITS_MASK(24, 29);
	 * a_sig->fixed_mask = BITS_MASK(30, 31);
	 * a_sig->fixed_value = 0x2 << 30;
	 *
	 * This is tricky since the free-running flag might not have been
	 * parsed by the crate when this is called... So the crate will have
	 * to defer the barrier-check until all configs are parsed.
	 */
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(16, 23),
		BITS_MASK(28, 31),
		0x2 << 30)
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		DMA_FILLER)
	MODULE_SIGNATURE_END(a_array, a_num)
}

void
mesytec_mdpp_init_fast(struct Crate *a_crate, struct MesytecMdppModule
    *a_mdpp)
{
	enum Keyword c_nim[] = {
		KW_OFF, KW_CBUS, KW_BUSY, KW_DATA_THRESHOLD,
		KW_EVENT_THRESHOLD, KW_NONE,
		KW_ON, KW_NONE,
		KW_OFF, KW_TRIG1, KW_RESET, KW_NONE,
		KW_OFF, KW_SYNC, KW_NONE,
		KW_OFF, KW_TRIG0
	};
	enum Keyword c_ecl[] = {
		KW_OFF, KW_BUSY, KW_DATA_THRESHOLD, KW_EVENT_THRESHOLD,
		KW_NONE,
		KW_OFF, KW_RESET, KW_NONE,
		KW_OFF, KW_SYNC, KW_TRIG1, KW_NONE,
		KW_OFF, KW_TRIG0
	};
	enum Keyword c_term[] = {
		KW_OFF, KW_ON, KW_NONE,
		KW_OFF, KW_ON, KW_NONE,
		KW_OFF, KW_ON, KW_NONE,
		KW_OFF, KW_ON, KW_NONE
	};
	struct ModuleGate gate;
	double init_sleep;
	unsigned nim[5];
	unsigned ecl[4];
	size_t i;
	int has_input_clock;
	int has_input_reset;
	int has_input_t0;
	int has_input_t1;

	LOGF(info)(LOGL, NAME" init_fast {");

	mesytec_mxdc32_init_fast(a_crate, &a_mdpp->mxdc32, 0);

	init_sleep = mesytec_mxdc32_sleep_get(&a_mdpp->mxdc32);

	/* Thresholds. */
	config_get_int_array(a_mdpp->config.threshold,
	    sizeof *a_mdpp->config.threshold * a_mdpp->config.ch_num,
	    sizeof *a_mdpp->config.threshold,
	    a_mdpp->mxdc32.module.config, KW_THRESHOLD, CONFIG_UNIT_NONE, 0,
	    65535);
	thresholds_set(a_mdpp, a_mdpp->config.threshold);

	/* Use software gate. */
	module_gate_get(&gate, a_mdpp->mxdc32.module.config,
	    -25600.0, 25600.0, 0.0, 25600.0);
	a_mdpp->config.gate_offset =
	    ceil((gate.time_after_trigger_ns + 25600.0) / 25.0 * 16.0);
	a_mdpp->config.gate_width = ceil(gate.width_ns / 25.0 * 16.0);
	LOGF(verbose)(LOGL, "Trigger window=(0x%04x,0x%04x).",
	    a_mdpp->config.gate_offset, a_mdpp->config.gate_width);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, win_start,
	    a_mdpp->config.gate_offset);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, win_width,
	    a_mdpp->config.gate_width);

	has_input_clock = 0;
	has_input_reset = 0;
	has_input_t0 = 0;
	has_input_t1 = 0;

	/* NIM I/O. */
	CONFIG_GET_FLEX_KEYWORD_ARRAY(a_mdpp->config.nim,
	    a_mdpp->mxdc32.module.config, KW_NIM, c_nim);
	has_input_clock |= (KW_SYNC  == a_mdpp->config.nim[3]) << 0;
	has_input_reset |= (KW_RESET == a_mdpp->config.nim[2]) << 0;
	has_input_t0    |= (KW_TRIG0 == a_mdpp->config.nim[4]) << 0;
	has_input_t1    |= (KW_TRIG1 == a_mdpp->config.nim[2]) << 0;
	switch (a_mdpp->config.nim[0]) {
	case KW_OFF:             nim[0] = 0; break;
	case KW_CBUS:            nim[0] = 1; break;
	case KW_BUSY:            nim[0] = 4; break;
	case KW_DATA_THRESHOLD:  nim[0] = 8; break;
	case KW_EVENT_THRESHOLD: nim[0] = 9; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	nim[1] = 1;
	switch (a_mdpp->config.nim[2]) {
	case KW_OFF:             nim[2] = 0; break;
	case KW_TRIG1:           nim[2] = 1; break;
	case KW_RESET:           nim[2] = 2; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	switch (a_mdpp->config.nim[3]) {
	case KW_OFF:             nim[3] = 0; break;
	case KW_SYNC:            nim[3] = 2; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	switch (a_mdpp->config.nim[4]) {
	case KW_OFF:             nim[4] = 0; break;
	case KW_TRIG0:           nim[4] = 1; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, nim0, nim[0]);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, nim2, nim[2]);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, nim3, nim[3]);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, nim4, nim[4]);

	/* ECL I/O. */
	CONFIG_GET_FLEX_KEYWORD_ARRAY(a_mdpp->config.ecl,
	    a_mdpp->mxdc32.module.config, KW_ECL, c_ecl);
	has_input_clock |= (KW_SYNC  == a_mdpp->config.ecl[2]) << 1;
	has_input_reset |= (KW_RESET == a_mdpp->config.ecl[1]) << 1;
	has_input_t0    |= (KW_TRIG0 == a_mdpp->config.ecl[3]) << 1;
	has_input_t1    |= (KW_TRIG1 == a_mdpp->config.ecl[2]) << 1;
	switch (a_mdpp->config.ecl[0]) {
	case KW_OFF:             ecl[0] = 0; break;
	case KW_BUSY:            ecl[0] = 4; break;
	case KW_DATA_THRESHOLD:  ecl[0] = 8; break;
	case KW_EVENT_THRESHOLD: ecl[0] = 9; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	switch (a_mdpp->config.ecl[1]) {
	case KW_OFF:             ecl[1] = 0; break;
	case KW_RESET:           ecl[1] = 1; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	switch (a_mdpp->config.ecl[2]) {
	case KW_OFF:             ecl[2] = 0; break;
	case KW_SYNC:            ecl[2] = 1; break;
	case KW_TRIG1:           ecl[2] = 2; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	switch (a_mdpp->config.ecl[3]) {
	case KW_OFF:             ecl[3] = 0; break;
	case KW_TRIG0:           ecl[3] = 1; break;
	default: log_die(LOGL, "This cannot happen."); break;
	}
	CONFIG_GET_KEYWORD_ARRAY(a_mdpp->config.ecl_term,
	    a_mdpp->mxdc32.module.config, KW_USE_TERMINATION, c_term);
	for (i = 0; i < LENGTH(ecl); ++i) {
		if (KW_ON == a_mdpp->config.ecl_term[i]) {
			ecl[i] |= 0x100;
		}
	}
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, ecl0, ecl[0]);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, ecl1, ecl[1]);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, ecl2, ecl[2]);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, ecl3, ecl[3]);

#define DUP_INPUT(name) do { \
	if (3 == has_input_##name) { \
		log_die(LOGL, "Two " #name " inputs configured!"); \
	} \
} while (0)
	DUP_INPUT(clock);
	DUP_INPUT(reset);
	DUP_INPUT(t0);
	DUP_INPUT(t1);

	/* Check trigger setup. */
#define CHECK_TRIG(tnum, sett) do { \
	if (sett == a_mdpp->config.trigger_input && \
	    !has_input_t##tnum) { \
		log_die(LOGL, "T" #tnum " chosen as trigger, " \
		    "but no T" #tnum " conf:ed!"); \
	} \
} while (0)
	CHECK_TRIG(0, 35);
	CHECK_TRIG(1, 36);

	/* Clock setup. */
	a_mdpp->config.use_ext_clk = config_get_boolean(
	    a_mdpp->mxdc32.module.config, KW_CLOCK_INPUT);
	LOGF(info)(LOGL, "External clock = %s.",
	    a_mdpp->config.use_ext_clk ? "yes" : "no");
	if (a_mdpp->config.use_ext_clk) {
		int srcs;

		if (!has_input_clock) {
			log_die(LOGL, "User wanted external clock, but no "
			    "NIM/ECL conf:ed with clock!");
		}
		srcs = 1;
		if (!has_input_reset) {
			LOGF(info)(LOGL, "External clock enabled, "
			    "but not external reset!");
		} else {
			LOGF(verbose)(LOGL, "External clock reset enabled.");
			srcs |= 2;
		}
		/* External src & maybe reset. */
		MAP_WRITE(a_mdpp->mxdc32.sicy_map, ts_sources, srcs);

		LOGF(info)(LOGL, "Events will save timestamps, "
		    "not event counters.");
		MAP_WRITE(a_mdpp->mxdc32.sicy_map, marking_type, 1);
	}

	/* First hit. */
	a_mdpp->config.do_first_hit = config_get_boolean(
	    a_mdpp->mxdc32.module.config, KW_ONLY_FIRST_HIT);
	LOGF(info)(LOGL, "First hit only = %s.",
	    a_mdpp->config.do_first_hit ? "yes" : "no");
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, first_hit,
	    a_mdpp->config.do_first_hit);

	/* Monitor settings. */
	a_mdpp->config.monitor_on = config_get_boolean(
	    a_mdpp->mxdc32.module.config, KW_MONITOR_ON);
	a_mdpp->config.monitor_channel = config_get_int32(
	    a_mdpp->mxdc32.module.config,
	    KW_MONITOR_CHANNEL, CONFIG_UNIT_NONE,
	    0, a_mdpp->config.ch_num - 1);
	a_mdpp->config.monitor_wave = config_get_int32(
	    a_mdpp->mxdc32.module.config,
	    KW_MONITOR_WAVE, CONFIG_UNIT_NONE, 0, 3);
	if (a_mdpp->config.monitor_on) {
		LOGF(info)(LOGL, "Monitor ch = %u.",
		    a_mdpp->config.monitor_channel);
		LOGF(info)(LOGL, "Monitor wave = %u.",
		    a_mdpp->config.monitor_wave);
	}

	/* Make sure nim=() doesn't clash with the monitor. */
	if (a_mdpp->config.monitor_on &&
	    (KW_OFF != a_mdpp->config.nim[2] ||
	     KW_OFF != a_mdpp->config.nim[3])) {
		log_die(LOGL, "NIM 2/3 IO's collide with monitoring!");
	}

	MAP_WRITE(a_mdpp->mxdc32.sicy_map, monitor_on,
	    a_mdpp->config.monitor_on);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, set_mon_channel,
	    a_mdpp->config.monitor_channel);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, set_wave,
	    a_mdpp->config.monitor_wave);

	/* Pulser settings. */
	a_mdpp->config.pulser_enabled = config_get_boolean(
	    a_mdpp->mxdc32.module.config, KW_PULSER_ENABLED);
	a_mdpp->config.pulser_amplitude = config_get_int32(
	    a_mdpp->mxdc32.module.config, KW_PULSER_AMPLITUDE,
	    CONFIG_UNIT_NONE, 0, 0xfff);
	LOGF(verbose)(LOGL, "Pulser status=%s.",
	    a_mdpp->config.pulser_enabled ? "on" : "off");
	LOGF(verbose)(LOGL, "Pulser amplitude=%d.",
	    a_mdpp->config.pulser_amplitude);
	time_sleep(init_sleep);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, pulser_status,
	    a_mdpp->config.pulser_enabled);
	time_sleep(init_sleep);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, pulser_amplitude,
	    a_mdpp->config.pulser_amplitude);
	time_sleep(init_sleep);

	LOGF(info)(LOGL, NAME" init_fast }");
}

void
mesytec_mdpp_init_slow(struct Crate const *a_crate, struct MesytecMdppModule
    *a_mdpp)
{
	uint16_t reset_time[8];
	double init_sleep;
	unsigned i;
	uint16_t trig_out, trig_source, trig_source_2;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_slow {");

	mesytec_mxdc32_init_slow(&a_mdpp->mxdc32);

	LOGF(info)(LOGL, NAME" Firmware = 0x%04x.",
	    MAP_READ(a_mdpp->mxdc32.sicy_map, firmware_revision));

	/*
	 * Trigger config can be set ambiguously, so we don't allow setting
	 * that online.
	 * TODO: Eh why not?
	 */

	a_mdpp->config.trigger_output = config_get_int32(
	    a_mdpp->mxdc32.module.config, KW_TRIGGER_OUTPUT, CONFIG_UNIT_NONE,
	    -1, a_mdpp->config.ch_num + 2);
	if (16 == a_mdpp->config.ch_num) {
		a_mdpp->config.trigger_output =
		    MIN(a_mdpp->config.trigger_output, 16);
	}
	LOGF(info)(LOGL, "Trigger_output (TO) = %d.",
	    a_mdpp->config.trigger_output);

	if (-1 == a_mdpp->config.trigger_output) {
		trig_out = 0;
	} else if (a_mdpp->config.trigger_output <
	    (int)a_mdpp->config.ch_num) {
		trig_out = 0x80 | (a_mdpp->config.trigger_output << 2);
                /*
		 * TODO: The behaviour of output trigger is not following as
		 * described in the manual but setting every four channels
		 * like the lines below.
		 */
                /*
		MAP_WRITE(a_mdpp->mxdc32.sicy_map, trigger_output,
		    0x80 | (a_mdpp->config.trigger_output));
                */
	} else if ((int)a_mdpp->config.ch_num ==
	    a_mdpp->config.trigger_output) {
		trig_out = 0x100;
	} else if (32 == a_mdpp->config.ch_num &&
	    33 == a_mdpp->config.trigger_output) {
		trig_out = 0x200;
	} else if (32 == a_mdpp->config.ch_num &&
	    34 == a_mdpp->config.trigger_output) {
		trig_out = 0x300;
	} else {
		log_die(LOGL,
		    "Invalid trigger-output config! "
		    "ch_num=%d trigger_output=%d.",
		    a_mdpp->config.ch_num, a_mdpp->config.trigger_output);
	}
	LOGF(info)(LOGL, "Trig_out = 0x%04x.", trig_out);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, trigger_output, trig_out);

	a_mdpp->config.trigger_input = config_get_int32(
	    a_mdpp->mxdc32.module.config, KW_TRIGGER_INPUT, CONFIG_UNIT_NONE,
	    -1, a_mdpp->config.ch_num + 4);
	LOGF(info)(LOGL, "Trigger_input (TI) = %d.",
	    a_mdpp->config.trigger_input);

	if (-1 == a_mdpp->config.trigger_input) {
		trig_source = 0;
	} else if (a_mdpp->config.trigger_input <
	    (int)a_mdpp->config.ch_num) {
		trig_source = 0x80 | a_mdpp->config.trigger_input << 2;
	} else if ((int)a_mdpp->config.ch_num ==
	    a_mdpp->config.trigger_input) {
		trig_source = 0x100;
	} else if (32 == a_mdpp->config.ch_num &&
	    33 == a_mdpp->config.trigger_input) {
		trig_source = 0x200;
	} else if (32 == a_mdpp->config.ch_num &&
	    34 == a_mdpp->config.trigger_input) {
		trig_source = 0x300;
	} else if ((16 == a_mdpp->config.ch_num &&
	            17 == a_mdpp->config.trigger_input) ||
	           (32 == a_mdpp->config.ch_num &&
	            35 == a_mdpp->config.trigger_input)) {
		trig_source = 1;
	} else if ((16 == a_mdpp->config.ch_num &&
	            18 == a_mdpp->config.trigger_input) ||
	           (32 == a_mdpp->config.ch_num &&
	            36 == a_mdpp->config.trigger_input)) {
		trig_source = 2;
	} else {
		log_die(LOGL,
		    "Invalid trigger-input config! "
		    "ch_num=%d trigger_input=%d.",
		    a_mdpp->config.ch_num, a_mdpp->config.trigger_input);
	}
	LOGF(info)(LOGL, "Trig_source = 0x%04x.", trig_source);
	MAP_WRITE(a_mdpp->mxdc32.sicy_map, trig_source, trig_source);

	if (KW_MESYTEC_MDPP16SCP == a_mdpp->mxdc32.module.type ||
	    KW_MESYTEC_MDPP16QDC == a_mdpp->mxdc32.module.type) {
		a_mdpp->config.trigger_mask = config_get_bitmask(
		    a_mdpp->mxdc32.module.config, KW_TRIGGER_CHANNEL,
		    0, a_mdpp->config.ch_num - 1);
		LOGF(info)(LOGL, "Trigger_channel mask = 0x%04x.",
		    a_mdpp->config.trigger_mask);

		if (-1 != a_mdpp->config.trigger_input &&
		    0 != a_mdpp->config.trigger_mask) {
			log_die(LOGL, "One of trigger_input and "
			    "trigger_channel must be disabled!");
		}
		if (-1 == a_mdpp->config.trigger_input &&
		    0 == a_mdpp->config.trigger_mask) {
			log_die(LOGL, "No trigger source (T0/T1, bank, "
			    "channel, or channel mask) confed, choose at "
			    "least one!");
		}

		trig_source_2 = a_mdpp->config.trigger_mask;
		LOGF(info)(LOGL, "Trig_source_2 = 0x%04x.",
		    trig_source_2);
		MAP_WRITE(a_mdpp->mxdc32.sicy_map, trig_source_2,
		    trig_source_2);
	}

	CONFIG_GET_UINT_ARRAY(reset_time, a_mdpp->mxdc32.module.config,
	    KW_RESET_TIME, CONFIG_UNIT_NS, 200, 65535);
	init_sleep = mesytec_mxdc32_sleep_get(&a_mdpp->mxdc32);
	for (i = 0; 8 > i; ++i) {
		MAP_WRITE(a_mdpp->mxdc32.sicy_map, reset_time,
		    reset_time[0] / 12.5);
		time_sleep(init_sleep);
	}

	LOGF(info)(LOGL, NAME" init_slow }");
}

void
mesytec_mdpp_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_mdpp_parse_data(struct Crate *a_crate, struct MesytecMdppModule
    *a_mdpp, struct EventConstBuffer const *a_event_buffer, int
    a_do_pedestals, int a_is_eob_old)
{
	return mesytec_mxdc32_parse_data(a_crate, &a_mdpp->mxdc32,
	    a_event_buffer, a_do_pedestals, a_is_eob_old,
	    a_mdpp->config.use_ext_clk);
}

int
mesytec_mdpp_post_init(struct MesytecMdppModule *a_mdpp)
{
	return mesytec_mxdc32_post_init(&a_mdpp->mxdc32);
}

#if NCONF_mMAP_bCMVLC
void
mesytec_mdpp_cmvlc_init(struct MesytecMdppModule *a_mdpp,
    struct cmvlc_stackcmdbuf *a_stack, int a_dt)
{
	mesytec_mxdc32_cmvlc_init(&a_mdpp->mxdc32, a_stack, a_dt);
}

uint32_t
mesytec_mdpp_cmvlc_fetch_dt(struct MesytecMdppModule *a_mdpp,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	return mesytec_mxdc32_cmvlc_fetch_dt(&a_mdpp->mxdc32,
	    a_in_buffer, a_in_remain, a_in_used);
}

uint32_t
mesytec_mdpp_cmvlc_fetch(struct Crate *a_crate,
    struct MesytecMdppModule *a_mdpp, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	return mesytec_mxdc32_cmvlc_fetch(a_crate, &a_mdpp->mxdc32,
	    a_event_buffer, a_in_buffer, a_in_remain, a_in_used);
}
#endif

uint32_t
mesytec_mdpp_readout(struct Crate *a_crate, struct MesytecMdppModule *a_mdpp,
    struct EventBuffer *a_event_buffer, int a_is_eob_old)
{
	return mesytec_mxdc32_readout(a_crate, &a_mdpp->mxdc32,
	    a_event_buffer, a_is_eob_old, a_mdpp->config.use_ext_clk);
}

uint32_t
mesytec_mdpp_readout_dt(struct Crate *a_crate, struct MesytecMdppModule
    *a_mdpp)
{
	return mesytec_mxdc32_readout_dt(a_crate, &a_mdpp->mxdc32);
}

uint32_t
mesytec_mdpp_readout_shadow(struct MesytecMdppModule *a_mdpp, struct
    EventBuffer *a_event_buffer)
{
	return mesytec_mxdc32_readout_shadow(&a_mdpp->mxdc32, a_event_buffer,
	    a_mdpp->config.use_ext_clk);
}

void
mesytec_mdpp_use_pedestals(struct MesytecMdppModule *a_mdpp)
{
	/*
	 * MESYTEC_MXDC32_USE_PEDESTALS(Mdpp) cannot be used
	 * because there is no direct channel register access
	 */
	uint16_t arr[32];
	struct Pedestal const *pedestal_array;
	size_t i;

	if (!a_mdpp->config.do_auto_pedestals) {
		return;
	}
	LOGF(verbose)(LOGL, NAME" use_pedestals {");
	pedestal_array = a_mdpp->mxdc32.module.pedestal.array;
	if (a_mdpp->config.ch_num != a_mdpp->mxdc32.module.pedestal.array_len)
	{
		log_die(LOGL, "Nurdlib is broken!");
	}
	for (i = 0; i < a_mdpp->config.ch_num; ++i) {
		arr[i] = pedestal_array[i].threshold;
	}
	thresholds_set(a_mdpp, arr);
	LOGF(verbose)(LOGL, NAME" use_pedestals }");
}

void
mesytec_mdpp_zero_suppress(struct MesytecMdppModule *a_mdpp, int a_yes)
{
	/* This module cannot ignore thresholds, so no = 0. */
	uint16_t arr[32];
	size_t i;

	if (!a_mdpp->config.do_auto_pedestals) {
		return;
	}
	LOGF(verbose)(LOGL, NAME" zero_suppress {");
	for (i = 0; i < a_mdpp->config.ch_num; ++i) {
		arr[i] = a_yes ? a_mdpp->config.threshold[i] : 0;
	}
	thresholds_set(a_mdpp, arr);
	LOGF(verbose)(LOGL, NAME" zero_suppress }");
}

void
thresholds_set(struct MesytecMdppModule *a_mdpp, uint16_t const *a_array)
{
	double init_sleep;
	unsigned i;

	LOGF(verbose)(LOGL, NAME" thresholds_set {");

	init_sleep = mesytec_mxdc32_sleep_get(&a_mdpp->mxdc32);

	if (16 == a_mdpp->config.ch_num) {
		for (i = 0; a_mdpp->config.ch_num / 2 > i; ++i) {
			uint16_t t0, t1;

			t0 = a_array[i * 2 + 0];
			t1 = a_array[i * 2 + 1];
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, select_chan_pair,
			    i);
			time_sleep(init_sleep);
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, threshold0, t0);
			time_sleep(init_sleep);
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, threshold1, t1);
			time_sleep(init_sleep);
			LOGF(verbose)(LOGL,
			    "thr0[%u]=0x%04x thr1[%u]=0x%04x.",
			    i, t0, i, t1);
		}
	} else if (32 == a_mdpp->config.ch_num) {
		for (i = 0; a_mdpp->config.ch_num / 4 > i; ++i) {
			uint16_t t0, t1, t2, t3;

			t0 = a_array[i * 4 + 0];
			t1 = a_array[i * 4 + 1];
			t2 = a_array[i * 4 + 2];
			t3 = a_array[i * 4 + 3];
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, select_chan_pair,
			    i);
			time_sleep(init_sleep);
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, threshold0, t0);
			time_sleep(init_sleep);
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, threshold1, t1);
			time_sleep(init_sleep);
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, threshold2, t2);
			time_sleep(init_sleep);
			MAP_WRITE(a_mdpp->mxdc32.sicy_map, threshold3, t3);
			time_sleep(init_sleep);
			LOGF(verbose)(LOGL,
			    "thr0[%u]=0x%04x thr1[%u]=0x%04x "
			    "thr2[%u]=0x%04x thr3[%u]=0x%04x.",
			    i, t0, i, t1,
			    i, t2, i, t3);
		}
	}

	LOGF(verbose)(LOGL, NAME" thresholds_set }");
}
