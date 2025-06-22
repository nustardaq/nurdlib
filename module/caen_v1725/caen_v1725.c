/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2025
 * Håkan T Johansson
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

#include <module/caen_v1725/caen_v1725.h>
#include <module/caen_v1725/internal.h>
#include <sched.h>
#include <module/caen_v1725/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <nurdlib/serialio.h>
#include <util/assert.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/string.h>
#include <util/time.h>
#include <module/map/map_cmvlc.h>

#define NAME "Caen v1725"

#define DMA_FILLER 0x17251725
#define NO_DATA_TIMEOUT 1.0

#define CFG_TRIGGER_OVERLAP   (1 << 1)
#define CFG_TEST_PATTERN      (1 << 3)
#define CFG_SELF_TRIGGER_NEG  (1 << 6)

#define CTL_OPTICAL_INTERRUPT (1 << 3)
#define CTL_BERR_ENABLE       (1 << 4)
#define CTL_ALIGN64           (1 << 5)
#define CTL_ADDRESS_RELOC     (1 << 6)
#define CTL_INTERRUPT_ROAK    (1 << 7)
#define CTL_BLT_EXTENDED      (1 << 8)

#define ACQ_MODE_SW           (0)
#define ACQ_MODE_S_IN         (1)
#define ACQ_MODE_FIRST        (2)
#define ACQ_MODE_LVDS         (3)
#define ACQ_START             (1 <<  2)
#define ACQ_ALL_TRIGGERS      (1 <<  3)
#define ACQ_ALMOST_FULL       (1 <<  5)
#define ACQ_EXTERNAL_CLOCK    (1 <<  6)
#define ACQ_BUSY_IO           (1 <<  8)
#define ACQ_VETO_IO           (1 <<  9)
#define ACQ_START_ON_RISING   (1 << 11)
#define ACQ_INHIBIT_TRG_OUT   (1 << 12)

#define SET_THRESHOLDS(v1725, array) \
    set_thresholds(v1725, array, LENGTH(array))

MODULE_PROTOTYPES(caen_v1725);
static void	set_thresholds(struct CaenV1725Module *, uint32_t const *,
    size_t);
static void	caen_v1725_cmvlc_init(struct Module *,
    struct cmvlc_stackcmdbuf *, int);
static uint32_t	caen_v1725_cmvlc_fetch_dt(struct Module *,
    const uint32_t *, uint32_t, uint32_t *);
static uint32_t	caen_v1725_cmvlc_fetch(struct Crate *, struct
    Module *, struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *);


uint32_t
caen_v1725_check_empty(struct Module *a_module)
{
	struct CaenV1725Module *v1725;
	uint32_t events;

	/* This function cannot really be used with DPP-PSD:
	 *
	 * DPP-PSD has no regsiter telling the number of stored
	 * events, only the size of the next event.  But that
	 * sometimes report a non-zero event size even through no data
	 * is present.
	 */
	LOGF(spam)(LOGL, NAME" check_empty {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	/* events = MAP_READ(v1725->sicy_map, event_stored); */
	(void) v1725;
	events = 0;
	LOGF(spam)(LOGL, NAME" check_empty(events=%u) }", events);
	return events > 0 ? CRATE_READOUT_FAIL_DATA_TOO_MUCH : 0;
}

struct Module *
caen_v1725_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	enum Keyword const c_version[] = {KW_DPP_PSD};
	struct CaenV1725Module *v1725;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(v1725);
	v1725->module.event_max = 1;

	v1725->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", v1725->address);

	v1725->version = CONFIG_GET_KEYWORD(a_block, KW_VERSION, c_version);

	v1725->module.event_counter.mask = BITS_MASK_TOP(23);

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)v1725;
}

void
caen_v1725_deinit(struct Module *a_module)
{
	struct CaenV1725Module *v1725;

	LOGF(info)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	map_unmap(&v1725->sicy_map);
	map_unmap(&v1725->dma_map);
	LOGF(info)(LOGL, NAME" deinit }");
}

void
caen_v1725_destroy(struct Module *a_module)
{
	(void)a_module;
}

struct Map *
caen_v1725_get_map(struct Module *a_module)
{
	struct CaenV1725Module *v1725;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return v1725->sicy_map;
}

void
caen_v1725_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature }");
}

/*
 * The register value corresponds to clk_mul sample clock periods.
 * The register has active bits [top_bit..0], i.e. one more than top_bit.
 */
#define PREPARE_TIME_CONFIG(u32, cfg, n, clk_mul, top_bit) do {\
	double setting[n];\
	size_t j;\
	CONFIG_GET_DOUBLE_ARRAY(setting, v1725->module.config,\
	    cfg, CONFIG_UNIT_NS, 0,\
	    BITS_MASK_TOP(top_bit) * clk_mul * v1725->period_ns);\
	for (j = 0; j < LENGTH(setting); ++j) {\
		u32[j] = CLAMP(setting[j] / clk_mul / v1725->period_ns,\
		    0, BITS_MASK_TOP(top_bit));\
	}\
} while (0);

#define APPLY_TIME_CONFIG(reg, cfg, n, clk_mul, top_bit) do {\
	uint32_t u32[n];\
	size_t i;\
	PREPARE_TIME_CONFIG(u32, cfg, n, clk_mul, top_bit);\
	for (i = 0; i < LENGTH(u32); ++i) {\
		MAP_WRITE(v1725->sicy_map, reg(i), u32[i]);\
	}\
} while (0);

/*
 * Upper limit per channel could be dynamic-range dependent.
 * Handled by clamping.
 */
#define PREPARE_MV_CONFIG(u32, cfg, n, top_bit) do {\
	double setting[n];\
	size_t j;\
	CONFIG_GET_DOUBLE_ARRAY(setting, v1725->module.config,\
	    cfg, CONFIG_UNIT_MV, 0, 2.0);\
	for (j = 0; j < LENGTH(setting); ++j) {\
		u32[j] = CLAMP(setting[j]\
		    / (dyn_range_setting[j] ? 0.03 : 0.12),\
		    0, BITS_MASK_TOP(top_bit));\
	}\
} while (0);

#define APPLY_MV_CONFIG(reg, cfg, n, top_bit) do {\
	uint32_t u32[n];\
	size_t i;\
	PREPARE_MV_CONFIG(u32, cfg, n, top_bit);\
	for (i = 0; i < LENGTH(u32); ++i) {\
		MAP_WRITE(v1725->sicy_map, reg(i), u32[i]);\
	}\
} while (0);

#define PREPARE_CONFIG(u32, cfg, top_bit) do {\
	CONFIG_GET_INT_ARRAY(u32, v1725->module.config,\
	    cfg, CONFIG_UNIT_NONE, 0, BITS_MASK_TOP(top_bit));\
} while (0);

#define APPLY_CONFIG(reg, cfg, n, top_bit) do {\
	uint32_t u32[n];\
	size_t i;\
	PREPARE_CONFIG(u32, cfg, top_bit);\
	for (i = 0; i < LENGTH(u32); ++i) {\
		MAP_WRITE(v1725->sicy_map, reg(i), u32[i]);\
	}\
} while (0);

#define PREPARE_M1_CONFIG(i32, cfg, top_bit) do {\
	CONFIG_GET_INT_ARRAY(i32, v1725->module.config,\
	    cfg, CONFIG_UNIT_NONE, -1, BITS_MASK_TOP(top_bit));\
} while (0);

#define APPLY_M1_CONFIG(reg, cfg, n, top_bit) do {\
	int32_t i32[n];\
	size_t i;\
	PREPARE_M1_CONFIG(i32, cfg, top_bit);\
	for (i = 0; i < LENGTH(i32); ++i) {\
		if (i32[i] < 0)\
			i32[i] = 0;\
		MAP_WRITE(v1725->sicy_map, reg(i), i32[i]);\
	}\
} while (0);

#define PREPARE_FRAC_CONFIG(u32, cfg, n, top_bit) do {\
	double setting[n];\
	size_t j;\
	CONFIG_GET_DOUBLE_ARRAY(setting, v1725->module.config,\
	    cfg, CONFIG_UNIT_NONE, 0, 1);\
	for (j = 0; j < LENGTH(setting); ++j) {\
		u32[j] = CLAMP(setting[j] * (1UL << ((top_bit)+1)),\
			       0, BITS_MASK_TOP(top_bit));\
	}\
} while (0);

#define APPLY_FRAC_CONFIG(reg, cfg, n, top_bit) do {\
	uint32_t u32[n];\
	size_t i;\
	PREPARE_FRAC_CONFIG(u32, cfg, n, top_bit);\
	for (i = 0; i < LENGTH(u32); ++i) {\
		MAP_WRITE(v1725->sicy_map, reg(i), u32[i]);\
	}\
} while (0);

#define PREPARE_KEYWORD_CONFIG(u32, cfg, keyword_list) do {\
	CONFIG_GET_KEYWORD_ARRAY(u32, v1725->module.config,\
	    cfg, keyword_list);\
} while (0);

#define PREPARE_BOOLEAN_CONFIG(u32, cfg) do {\
	enum Keyword c_boolean[] = {\
		KW_FALSE,\
		KW_TRUE,\
	};\
	size_t j;\
	CONFIG_GET_KEYWORD_ARRAY(u32, v1725->module.config,\
	    cfg, c_boolean);\
	for (j = 0; j < LENGTH(u32); ++j) {\
		switch (u32[j]) {\
		case KW_FALSE: u32[j] = 0; break;\
		case KW_TRUE: u32[j] = 1; break; \
		}\
	}\
} while (0);

int
caen_v1725_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1725Module *v1725;
	uint32_t dyn_range_setting[16];

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

	{
		double range[16];
		size_t i;

		CONFIG_GET_DOUBLE_ARRAY(range, v1725->module.config, KW_RANGE,
		    CONFIG_UNIT_V, 0.0, 2.0);
		LOGF(verbose)(LOGL, "Input ranges:");
		for (i = 0; i < LENGTH(range); ++i) {
			uint32_t u32;

			u32 = range[i] <= 0.5;
			dyn_range_setting[i] = u32;
			LOGF(verbose)(LOGL, " [%"PRIz"]=%fV (0x%08x).",
			    i, u32 ? 0.5 : 2.0, u32);
			/*
			MAP_WRITE(v1725->sicy_map, input_dynamic_range(i),
			    u32);
			*/
		}
	}
	APPLY_TIME_CONFIG(record_length, KW_SAMPLE_LENGTH, 8, 8, 13);
	APPLY_CONFIG(number_of_events_per_aggregate, KW_AGGREGATE_NUM, 8, 9);
	APPLY_TIME_CONFIG(pre_trigger, KW_PRETRIGGER_DELAY, 16, 4, 9);
	APPLY_M1_CONFIG(charge_zero_suppression_threshold, KW_ZERO_SUPPRESS,
		     16, 15);
	APPLY_TIME_CONFIG(short_gate_width,KW_GATE_SHORT, 16, 1, 11);
	APPLY_TIME_CONFIG(long_gate_width,KW_GATE_LONG, 16, 1, 15);
	APPLY_TIME_CONFIG(gate_offset,KW_GATE_OFFSET, 16, 1, 7);
	APPLY_CONFIG(fixed_baseline, KW_BASELINE_FIXED, 16, 13);
	APPLY_TIME_CONFIG(trigger_latency,KW_INTERNAL_TRIGGER_DELAY, 16, 4, 9);
	APPLY_TIME_CONFIG(shaped_trigger_width,KW_INTERNAL_TRIGGER_WIDTH,
			  16, 4, 9);
	APPLY_TIME_CONFIG(trigger_hold_off_width,KW_INTERNAL_TRIGGER_HOLDOFF,
			  16, 4, 15);
	APPLY_FRAC_CONFIG(threshold_for_the_psd_cut, KW_THRESHOLD_PSD, 16, 9);
	APPLY_MV_CONFIG(pur_gap_threshold, KW_THRESHOLD_PUR_GAP, 16, 11);
	APPLY_TIME_CONFIG(early_baseline_freeze,KW_BASELINE_FREEZE, 16, 4, 9);
	{
		uint32_t cfd_delay_u32[16];
		uint32_t cfd_fraction[16];
		uint32_t cfd_width[16];
		size_t i;

		PREPARE_TIME_CONFIG(cfd_delay_u32, KW_CFD_DELAY, 16, 1, 7);
		CONFIG_GET_INT_ARRAY(cfd_fraction, v1725->module.config,
				     KW_CFD_FRACTION,
				     CONFIG_UNIT_NONE, 25, 100);
		CONFIG_GET_INT_ARRAY(cfd_width, v1725->module.config,
				     KW_CFD_WIDTH,
				     CONFIG_UNIT_NONE, 1, 4);

		for (i = 0; i < LENGTH(cfd_delay_u32); ++i) {
			uint32_t u32;
			uint32_t fraction;

			fraction = (cfd_fraction[i] / 25) * 25;

			u32 =
			  cfd_delay_u32[i] |
			  fraction << 8 |
			  cfd_width[i] << 10;
			MAP_WRITE(v1725->sicy_map, cfd_settings(i), u32);
		}


	}
	{
#if 0  /* Removed from cfg/default/caen_v1725.cfg ? */
		double pwidth[16];
		size_t i;

		CONFIG_GET_DOUBLE_ARRAY(pwidth, v1725->module.config,
		    KW_PULSE_WIDTH, CONFIG_UNIT_NS, 0.0, 255 * 16.0);
		LOGF(verbose)(LOGL, "Discriminator pulse-widths:");
		for (i = 0; i < LENGTH(pwidth); ++i) {
			uint32_t u32;

			u32 = (pwidth[i] + v1725->period_ns - 1) /
			    v1725->period_ns;
			u32 = MIN(u32, 255);
			LOGF(verbose)(LOGL, " [%"PRIz"]=%uns (0x%08x).",
			    i, u32 * v1725->period_ns, u32);
#if 0 /* No longer in register list. */
			MAP_WRITE(v1725->sicy_map, channel_n_pulse_width(i),
			    u32);
#endif
		}
#endif
	}
	{
		uint32_t thr[16];

		PREPARE_MV_CONFIG(thr, KW_THRESHOLD, 16, 13);
		SET_THRESHOLDS(v1725, thr);
	}
#if 0  /* Changed implementation in cfg/default/caen_v1725.cfg ? */
	{
		uint8_t logic[8];
		size_t i;

		CONFIG_GET_UINT_ARRAY(logic, v1725->module.config,
		    KW_TRIGGER_METHOD, CONFIG_UNIT_NONE, 0, 3);
		LOGF(verbose)(LOGL, "Self-trigger logic:");
		for (i = 0; i < LENGTH(logic); ++i) {
			uint32_t u32;

			u32 = logic[i];
			LOGF(verbose)(LOGL, " [%"PRIz"]=0x%08x.", i, u32);
#if 0 /* No longer in register list. */
			MAP_WRITE(v1725->sicy_map,
			    couple_n_self_trigger_logic(i), u32);
#endif
		}
	}
#endif
	{
		uint16_t offset[16];
		double init_sleep;
		size_t i;

		CONFIG_GET_INT_ARRAY(offset, v1725->module.config,
		    KW_OFFSET, CONFIG_UNIT_NONE, 0, 0xffff);
		init_sleep = config_get_double(v1725->module.config,
		    KW_INIT_SLEEP, CONFIG_UNIT_S, 0, 10);
		LOGF(verbose)(LOGL, "DC offset (sleep=%fs):", init_sleep);
		for (i = 0; i < LENGTH(offset); ++i) {
			uint32_t u32;

			u32 = offset[i];
			LOGF(verbose)(LOGL, " [%"PRIz"]=0x%08x.", i, u32);
			/*
			MAP_WRITE(v1725->sicy_map, dc_offset(i), u32);
			*/
		}
		time_sleep(init_sleep);
	}
	{
		enum Keyword c_kw_clock[] = {KW_INTERNAL, KW_EXTERNAL};
		uint32_t acq;

		acq =
		    ACQ_MODE_S_IN |
		    ACQ_START |
		    ACQ_ALL_TRIGGERS |
		    ACQ_ALMOST_FULL |
		    /* ACQ_EXTERNAL_CLOCK | */
		    ACQ_BUSY_IO |
		    /* ACQ_VETO_IO | */
		    /* ACQ_START_ON_RISING | */
		    /* ACQ_INHIBIT_TRG_OUT | */
		    0;
		if (KW_EXTERNAL == CONFIG_GET_KEYWORD(v1725->module.config,
		    KW_CLOCK_INPUT, c_kw_clock)) {
			acq |= ACQ_EXTERNAL_CLOCK;
		}
		LOGF(verbose)(LOGL, "Acquisition control=0x%08x.", acq);
		/*
		MAP_WRITE(v1725->sicy_map, acquisition_control, acq);
		*/
	}

	{
		enum Keyword const c_kw_trig_in[] =
		    {KW_OFF, KW_LVDS, KW_LEMO};
		double width_dbl;
		uint32_t ch, width, level, in;
		uint32_t u32;

		ch = config_get_bitmask(v1725->module.config,
		    KW_TRIGGER_CHANNEL, 0, 15);
		/*
		 * v1725->period_ns is the sampling period.
		 * The trigger clock period is four times longer.
		 * Width in global_trigger_mask uses the trigger clock.
		 */
		width_dbl = config_get_double(v1725->module.config,
		    KW_MAJORITY_WIDTH, CONFIG_UNIT_NS, 0, 15 *
		    v1725->period_ns * 4);
		level = config_get_int32(v1725->module.config,
		    KW_MAJORITY_LEVEL, CONFIG_UNIT_NONE, 1, 16);
		in = CONFIG_GET_KEYWORD(v1725->module.config,
		    KW_TRIGGER_INPUT, c_kw_trig_in);
		width = (uint32_t) CLAMP(width_dbl / v1725->period_ns / 4,
		    0,15);
		switch (in) {
		case KW_OFF: in = 0; break;
		case KW_LVDS: in = 1 << 29; break;
		case KW_LEMO: in = 1 << 30; break;
		}
		u32 =
		    ch |
		    width << 20 |
		    (level - 1) << 24 |
		    in;
		LOGF(verbose)(LOGL, "Global trigger mask=0x%08x.", u32);
		/*
		MAP_WRITE(v1725->sicy_map, global_trigger_mask, u32);
		*/
	}
	{
		/* TODO: front_panel_trg_out_gpo_enable_mask. */
	}
	{
		enum Keyword const c_kw_lemo[] = {KW_NIM, KW_TTL};
		uint8_t lvds[4];
		uint32_t lemo, trg_out;
		uint32_t u32;

		lemo = CONFIG_GET_KEYWORD(v1725->module.config, KW_LEMO,
		    c_kw_lemo) == KW_TTL;
		trg_out = config_get_boolean(v1725->module.config,
		    KW_TRIGGER_OUTPUT);
		CONFIG_GET_INT_ARRAY(lvds, v1725->module.config, KW_LVDS,
		    CONFIG_UNIT_NONE, 0, 1);
		u32 =
		    lemo << 0 |
		    trg_out << 1 |
		    lvds[0] << 2 |
		    lvds[1] << 3 |
		    lvds[2] << 4 |
		    lvds[3] << 5 |
		    0;
		LOGF(verbose)(LOGL, "Front panel IO control=0x%08x.", u32);
		/*
		MAP_WRITE(v1725->sicy_map, front_panel_i_o_control, u32);
		*/
	}
	{
		v1725->channel_enable = config_get_bitmask(
		    v1725->module.config, KW_CHANNEL_ENABLE, 0, 15);
		LOGF(verbose)(LOGL, "Channel mask=0x%08x.",
		    v1725->channel_enable);
		/*
		MAP_WRITE(v1725->sicy_map, channel_enable_mask,
		    v1725->channel_enable);
		*/
	}
	/* DPP Algorithm Control */
	{
		double charge_dbl[16];
		uint32_t charge;
		uint32_t pedestal[16];
		uint32_t trigout_all[16];
		uint32_t discrimination[16];
		uint32_t pileup_trigout[16];
		uint32_t test_pulse[16];
		uint32_t test_pulse_freq[16];
		uint32_t baseline_restart[16];
		uint32_t test_pulse_polarity[16];
		uint32_t trigger_method[16];
		uint32_t baseline_average[16];
		uint32_t use_internal_trigger[16];
		int32_t charge_zero_suppression_threshold[16];
		uint32_t suppress_charge_zero;
		uint32_t suppress_pileup[16];
		uint32_t psd_cut_below[16];
		uint32_t psd_cut_above[16];
		uint32_t suppress_over_range[16];
		uint32_t trigger_hysteresis[16];
		uint32_t polarity_detection[16];
		size_t i;

		enum Keyword c_discrimination[] = {
			KW_CFD,
			KW_LED
		};
		enum Keyword c_trigger_method[] = {
			KW_INDEPENDENT,
			KW_COINCIDENCE,
			KW_ANTICOINCIDENCE,
		};
		enum Keyword c_polarity[] = {
			KW_POS,
			KW_NEG
		};

		/* Note: min/max values not checked vs. manual. */

		CONFIG_GET_DOUBLE_ARRAY(charge_dbl, v1725->module.config,
		    KW_CHARGE, CONFIG_UNIT_FC, 1.25, 5.12);
		PREPARE_BOOLEAN_CONFIG(pedestal, KW_PEDESTAL);
		PREPARE_BOOLEAN_CONFIG(trigout_all, KW_TRIGOUT_ALL);
		PREPARE_KEYWORD_CONFIG(discrimination, KW_DISCRIMINATION,
		    c_discrimination);
		PREPARE_BOOLEAN_CONFIG(pileup_trigout, KW_PILEUP_TRIGOUT);
		PREPARE_BOOLEAN_CONFIG(test_pulse, KW_TEST_PULSE);
		CONFIG_GET_INT_ARRAY(test_pulse_freq, v1725->module.config,
		    KW_TEST_PULSE_FREQ, CONFIG_UNIT_HZ, 500, 1000000);
		PREPARE_BOOLEAN_CONFIG(baseline_restart, KW_BASELINE_RESTART);
		PREPARE_KEYWORD_CONFIG(test_pulse_polarity,
		    KW_TEST_PULSE_POLARITY, c_polarity);
		PREPARE_KEYWORD_CONFIG(trigger_method, KW_TRIGGER_METHOD,
		    c_trigger_method);
		CONFIG_GET_INT_ARRAY(baseline_average, v1725->module.config,
		    KW_BASELINE_AVERAGE, CONFIG_UNIT_NONE, 16, 1024);
		PREPARE_BOOLEAN_CONFIG(use_internal_trigger,
		    KW_USE_INTERNAL_TRIGGER);
		PREPARE_M1_CONFIG(charge_zero_suppression_threshold,
		    KW_ZERO_SUPPRESS, 15);
		PREPARE_BOOLEAN_CONFIG(suppress_pileup, KW_SUPPRESS_PILEUP);
		PREPARE_BOOLEAN_CONFIG(psd_cut_below, KW_PSD_CUT_BELOW);
		PREPARE_BOOLEAN_CONFIG(psd_cut_above, KW_PSD_CUT_ABOVE);
		PREPARE_BOOLEAN_CONFIG(suppress_over_range,
		    KW_SUPPRESS_OVER_RANGE);
		PREPARE_BOOLEAN_CONFIG(trigger_hysteresis,
	            KW_TRIGGER_HYSTERESIS);
		PREPARE_BOOLEAN_CONFIG(polarity_detection,
		    KW_POLARITY_DETECTION);

		for (i = 0; i < LENGTH(charge_dbl); ++i) {
			uint32_t u32;
			double charge_norm_2V = charge_dbl[i];

			/*
			 * 0.5 V range has levels a factor four smaller
			 * than 2 V setting.
			 */
			if (dyn_range_setting[i])
				charge_norm_2V *= 4;
			if      (charge_norm_2V < 5)    charge = 0;
			else if (charge_norm_2V < 20)   charge = 1;
			else if (charge_norm_2V < 80)   charge = 2;
			else if (charge_norm_2V < 320)  charge = 3;
			else if (charge_norm_2V < 1280) charge = 4;
			else                            charge = 5;

			if      (test_pulse_freq[i] < 500)
				test_pulse_freq[i] = 0;
			else if (test_pulse_freq[i] < 5000)
				test_pulse_freq[i] = 1;
			else if (test_pulse_freq[i] < 50000)
				test_pulse_freq[i] = 2;
			else if (test_pulse_freq[i] < 500000)
				test_pulse_freq[i] = 3;

			if      (baseline_average[i] < 16)
				baseline_average[i] = 1;
			else if (baseline_average[i] < 64)
				baseline_average[i] = 2;
			else if (baseline_average[i] < 256)
				baseline_average[i] = 3;
			else if (baseline_average[i] < 1024)
				baseline_average[i] = 4;

			switch (discrimination[i]) {
			case KW_LED: discrimination[i] = 0; break;
			case KW_CFD: discrimination[i] = 1; break;
			}

			switch (trigger_method[i]) {
			case KW_INDEPENDENT:     trigger_method[i] = 0; break;
			case KW_COINCIDENCE:     trigger_method[i] = 1; break;
			case KW_ANTICOINCIDENCE: trigger_method[i] = 3; break;
			}

			switch (discrimination[i]) {
			case KW_LED: discrimination[i] = 0; break;
			case KW_CFD: discrimination[i] = 1; break;
			}

			if (charge_zero_suppression_threshold[i] >= 0)
			  suppress_charge_zero = 1;
			else
			  suppress_charge_zero = 0;

			u32 =
			    charge << 0 |
			    pedestal[i] << 4 |
			    trigout_all[i] << 5 |
			    discrimination[i] << 6 |
			    pileup_trigout[i] << 7 |
			    test_pulse[i] << 8 |
			    test_pulse_freq[i] << 9 |
			    baseline_restart[i] << 15 |
			    test_pulse_polarity[i] << 16 |
			    trigger_method[i] << 18 |
			    baseline_average[i] << 20 |
			    use_internal_trigger[i] << 24 |
			    suppress_charge_zero << 25 |
			    suppress_pileup[i] << 26 |
			    psd_cut_below[i] << 27 |
			    psd_cut_above[i] << 28 |
			    suppress_over_range[i] << 29 |
			    trigger_hysteresis[i] << 30 |
			    polarity_detection[i] << 31;

			LOGF(verbose)(LOGL, "DPP algo ctrl[%"PRIz"]=0x%08x.",
			    i, u32);
			MAP_WRITE(v1725->sicy_map, dpp_algorithm_control(i),
			    u32);
		}
	}
	/* DPP Algorithm Control 2 */
	{
	  uint32_t dummy_int_array8[8];
	  uint32_t dummy_int_array16[16];
	  enum Keyword dummy_keyword_array16[16];
	  enum Keyword dummy_keyword_array8[8];

	  enum Keyword c_boolean[] = {
	    KW_FALSE,
	    KW_TRUE,
	  };

	  /* Note: min/max values not checked vs. manual. */

	  /*
	   * Group-level settings.
	   */
	  /* Rename: shaped_trigger_mode? */
	  CONFIG_GET_INT_ARRAY(dummy_int_array8, v1725->module.config,
			       KW_SELF_TRIGGER_SOURCE,
			       CONFIG_UNIT_NONE, 0, 3);
	  CONFIG_GET_KEYWORD_ARRAY(dummy_keyword_array8, v1725->module.config,
				   KW_TRIGGER_SHAPED, c_boolean);
	  /* Should be keyword: */
	  CONFIG_GET_INT_ARRAY(dummy_int_array8, v1725->module.config,
			       KW_TRIGGER_VALIDATION,
			       CONFIG_UNIT_NONE, 0, 5);
	  /* Should be keyword?: */
	  /*
	   * To make things more interesting:
	   * From here, settings are per-channel.
	   */
	  CONFIG_GET_INT_ARRAY(dummy_int_array16, v1725->module.config,
			       KW_EXTRA_WORD,
			       CONFIG_UNIT_NONE, 0, 7);
	  CONFIG_GET_INT_ARRAY(dummy_int_array16, v1725->module.config,
			       KW_SMOOTHED,
			       CONFIG_UNIT_NONE, 0, 16);
	  CONFIG_GET_INT_ARRAY(dummy_int_array16, v1725->module.config,
			       KW_TRIGGER_RATE,
			       CONFIG_UNIT_NONE, 128, 1024);
	  CONFIG_GET_INT_ARRAY(dummy_int_array16, v1725->module.config,
			       KW_VETO_SOURCE,
			       CONFIG_UNIT_NONE, 0, 3);
	  /* Should be keyword? */
	  CONFIG_GET_INT_ARRAY(dummy_int_array16, v1725->module.config,
			       KW_PIGGY_TO_MOBO,
			       CONFIG_UNIT_NONE, 0, 3);
	  CONFIG_GET_KEYWORD_ARRAY(dummy_keyword_array16, v1725->module.config,
				   KW_MARK_SATURATED, c_boolean);
	  /* Missing: additional local trigger validation. */
	  CONFIG_GET_KEYWORD_ARRAY(dummy_keyword_array16, v1725->module.config,
				   KW_USE_VETO, c_boolean);
	  /* Rename: veto_mode?: */
	  CONFIG_GET_KEYWORD_ARRAY(dummy_keyword_array8, v1725->module.config,
				   KW_VETO_DIRECT, c_boolean);
	  /* Missing: Reset timestamp on veto. */

	}
	/* Veto width. */
	{
	  uint32_t dummy_array16[16];

	  CONFIG_GET_INT_ARRAY(dummy_array16, v1725->module.config,
			       KW_VETO_WIDTH,
			       CONFIG_UNIT_NONE /* ns? */,
			       0, 1 /* ?? - figure*/);
	}

{
#define VME_WRITE_A32_D32(offset, value) do { \
	  map_sicy_write(v1725->sicy_map, 4/*MAP_MOD_W*/, 32, offset, value); \
	} while (0)

#define vme_base 0

  int i;

  /* Module setup. */
  VME_WRITE_A32_D32(vme_base + 0x8120, 0xffff); /* Channel enable. */

  VME_WRITE_A32_D32(vme_base + 0x8020, 128        ); /* Record length     */
  VME_WRITE_A32_D32(vme_base + 0x8028, 0          ); /* Input dynamic rng */
  VME_WRITE_A32_D32(vme_base + 0x8034, 64         ); /* Events per agg.   */
  VME_WRITE_A32_D32(vme_base + 0x8038, 250        ); /* Pre trigger       */
  VME_WRITE_A32_D32(vme_base + 0x803c, 0x0000010a ); /* CFD setting       */
  VME_WRITE_A32_D32(vme_base + 0x8044, 0          ); /* Charge zero thres */
  VME_WRITE_A32_D32(vme_base + 0x8054, 25         ); /* Short gate        */
  VME_WRITE_A32_D32(vme_base + 0x8058, 50         ); /* Long gate         */
  VME_WRITE_A32_D32(vme_base + 0x805c, 1          ); /* Gate offset       */
  VME_WRITE_A32_D32(vme_base + 0x8060, 200        ); /* Threshold         */
  VME_WRITE_A32_D32(vme_base + 0x8064, 0          ); /* Fixed baseline    */
  VME_WRITE_A32_D32(vme_base + 0x8070, 50         ); /* Trigger width     */
  VME_WRITE_A32_D32(vme_base + 0x8074, 50         ); /* Trigger holdoff   */
  VME_WRITE_A32_D32(vme_base + 0x8078, 0          ); /* PSD threshold     */
  VME_WRITE_A32_D32(vme_base + 0x807c, 0          ); /* PUR-GAP threshold */
  VME_WRITE_A32_D32(vme_base + 0x8080, 0x40150055 ); /* DPP algo ctrl     */
#if 1
  VME_WRITE_A32_D32(vme_base + 0x1f80, 0x40110055 ); /* DPP algo ctrl     */
#endif
  VME_WRITE_A32_D32(vme_base + 0x8084, 0x00002a75 ); /* DPP algo ctrl 2   */
  VME_WRITE_A32_D32(vme_base + 0x8098, 0x8000     ); /* DC offset         */
  VME_WRITE_A32_D32(vme_base + 0x80d4, 0          ); /* Veto width        */
  VME_WRITE_A32_D32(vme_base + 0x80d8, 5          ); /* Early bline freeze */

  for (i = 0; i < 8; i++)
    VME_WRITE_A32_D32(vme_base + 0x8180+4*i, 1 << i); /* Trig val mask. */

  /* Board configuration. */
  VME_WRITE_A32_D32(vme_base + 0x8004, 1);          /* Auto flush. */
  VME_WRITE_A32_D32(vme_base + 0x8004, 4);          /* Propagate trigger. */
  VME_WRITE_A32_D32(vme_base + 0x8004, 0x00020000); /* Extras recording. */
  VME_WRITE_A32_D32(vme_base + 0x8004, 0x000c0100); /* Per manual. */
#if 0
  VME_WRITE_A32_D32(vme_base + 0x8004, 0x00010000); /* Waveform recording. */
#endif
  VME_WRITE_A32_D32(vme_base + 0x800c, 8);          /* Aggregate org. */
#if 0
  VME_WRITE_A32_D32(vme_base + 0x8080, 0x100); /* Internal pulser. */
#endif
  VME_WRITE_A32_D32(vme_base + 0x810c, 0x01100000); /* Global trigger mask. */
  VME_WRITE_A32_D32(vme_base + 0x8110, 0x40000000); /* FP trigger out. */

  VME_WRITE_A32_D32(vme_base + 0xef00, 0x10);       /* Readout control. */
  VME_WRITE_A32_D32(vme_base + 0xef08, 0);          /* Board ID. */
#if 0
  /* This setting fails BLT. */
  VME_WRITE_A32_D32(vme_base + 0xef1c, 1);          /* Aggregate per BLT. */
#endif

  /* Enable acquisition. */
  VME_WRITE_A32_D32(vme_base + 0x8100, 0x04);
}

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
caen_v1725_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_MBLT,
		KW_FF,
		KW_NOBLT
	};
	struct CaenV1725Module *v1725;
	uint32_t result;
	uint16_t revision;

	result = 0;
	LOGF(info)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

	v1725->blt_mode = CONFIG_GET_KEYWORD(v1725->module.config,
	    KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "Using BLT mode = %s.",
	    keyword_get_string(v1725->blt_mode));

	v1725->do_berr = config_get_boolean(v1725->module.config, KW_BERR);
	LOGF(verbose)(LOGL, "Do BERR BLT=%s.", v1725->do_berr ? "yes" : "no");

	v1725->do_blt_ext = config_get_boolean(v1725->module.config,
	    KW_BLT_EXT);
	LOGF(verbose)(LOGL, "Do extended BLT=%s.",
	    v1725->do_berr ? "yes" : "no");

	v1725->sicy_map = map_map(v1725->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(scratch), MAP_POKE_REG(scratch), 0);

	/* TODO: Check geo before reset! */

	{
		uint32_t ff;

		ff = MAP_READ(v1725->sicy_map,
		    configuration_rom_board_form_factor);
		if (0 == ff) {
			/* VME64, we can write the nurdlib ID. */
			MAP_WRITE(v1725->sicy_map, board_id, v1725->geo);
		}
		v1725->geo = 0x1f & MAP_READ(v1725->sicy_map, board_id);
		LOGF(verbose)(LOGL, "GEO = %u.", v1725->geo);
		if (1 == ff) {
			/* VME64x, we have to remap the nurdlib ID. */
			crate_module_remap_id(a_crate, v1725->module.id,
			    v1725->geo);
		}
	}

	/* Reset, after GEO. */
	MAP_WRITE(v1725->sicy_map, software_reset, 1);
	MAP_WRITE(v1725->sicy_map, software_clear, 1);
	time_sleep(0.1);
	/*
	{
		uint32_t u32;

		u32 = CFG_TRIGGER_OVERLAP;
		LOGF(verbose)(LOGL, "Board configuration = 0x%08x.", u32);
		MAP_WRITE(v1725->sicy_map, board_configuration_bit_set, u32);
		MAP_WRITE(v1725->sicy_map, board_configuration_bit_clear,
		    ~u32);
	}
	*/

	SERIALIZE_IO;

	{
		unsigned i;

		for (i = 0; i < 16; ++i) {
			revision = MAP_READ(v1725->sicy_map,
			    amc_firmware_revision(i));
			LOGF(info)(LOGL,
			    "AMC Firmware revision = %d.%d (0x%04x).",
			    (0x0000ff00 & revision) >> 8,
			    0x000000ff & revision,
			    revision);
		}
	}

	revision = MAP_READ(v1725->sicy_map, roc_fpga_firmware_revision);
	LOGF(info)(LOGL, "ROC FPGA Firmware revision = %d.%d (0x%04x).",
	    (0x0000ff00 & revision) >> 8, 0x000000ff & revision, revision);

	/* Get module type and channel #. */
	{
		char const *ch_mem_size;
		uint32_t info;

		info = MAP_READ(v1725->sicy_map, board_info);
		switch (info & 0xff) {
		/*
		 * Manual UM4380, p. 11, section on register 'Record
		 * length' (0x1n20) states that 725 series is 4 ns
		 * sample period and 730 series is 2 ns sample period.
		 *
		 * When needed, the trigger clock period is four times
		 * longer than the sampling clock.
		 */
		case 0x0e: v1725->period_ns = 4; break;
		case 0x0b: v1725->period_ns = 2; break;
		default:
			log_error(LOGL,
			    "Invalid family-code in board-info 0x%08x.",
			    info);
			goto v1725_init_slow_done;
		}
		switch ((info >> 8) & 0xff) {
		case 0x01: ch_mem_size = "640 kS"; break;
		case 0x08: ch_mem_size = "5.12 MS"; break;
		default:
			log_error(LOGL,
			    "Invalid ch-mem-size in board-info 0x%08x.",
			    info);
			goto v1725_init_slow_done;
		}
		LOGF(verbose)(LOGL, "Ch-mem-size = %s.", ch_mem_size);
		switch ((info >> 16) & 0xff) {
		case 0x10: v1725->ch_num = 16; break;
		case 0x08: v1725->ch_num = 8; break;
		default:
			log_error(LOGL,
			    "Invalid ch-num in board-info 0x%08x.", info);
			goto v1725_init_slow_done;
		}
	}

	/* Enable BERR/SIGBUS to end DMA readout. */
	/*
	MAP_WRITE(v1725->sicy_map, readout_control,
	    (v1725->do_berr ? CTL_BERR_ENABLE : 0) |
	    CTL_ALIGN64 |
	    (v1725->do_blt_ext ? CTL_BLT_EXTENDED : 0));
	*/

	if (KW_NOBLT != v1725->blt_mode) {
		v1725->dma_map = map_map(v1725->address, 0x1000,
		    v1725->blt_mode, 1, 0,
		    MAP_POKE_REG(scratch), MAP_POKE_REG(scratch), 0);
	}

	result = 1;
v1725_init_slow_done:

	LOGF(info)(LOGL, NAME" init_slow(result=%d) }", result);
	return result;
}

void
caen_v1725_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v1725_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

void
caen_v1725_cmvlc_init(struct Module *a_module,
    struct cmvlc_stackcmdbuf *a_stack,
    int a_dt)
{
	struct CaenV1725Module *v1725;

	LOGF(verbose)(LOGL, NAME" cmvlc_init {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

#if NCONF_mMAP_bCMVLC
	if (a_dt) {
		/* Read event size. */
		cmvlc_stackcmd_vme_rw(a_stack, v1725->address + 0x814C, 0,
				      vme_rw_read, vme_user_A32, vme_D16);
		/* Twice, just to fill two words.  Fix fuser.c */
		cmvlc_stackcmd_vme_rw(a_stack, v1725->address + 0x814C, 0,
				      vme_rw_read, vme_user_A32, vme_D16);
	} else {
		/* Block transfer of data. */
		/* Note: 0x8000 MBLT (64-bit) words,is 0x10000 32-bit words. */
		cmvlc_stackcmd_vme_block(a_stack, v1725->address,
					 vme_rw_read_swap, vme_user_MBLT_A32,
					 0x8000);
	}
#else
	(void) a_module;
	(void) a_stack;
	(void) a_dt;
	(void) v1725;
#endif

	LOGF(verbose)(LOGL, NAME" cmvlc_init }");
}

uint32_t
caen_v1725_cmvlc_fetch_dt(struct Module *a_module,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
#if NCONF_mMAP_bCMVLC
	struct CaenV1725Module *v1725;
	uint32_t result;

	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	result = 0;

	if (a_in_remain < 2) {
		log_error(LOGL, "CAEN V1725: Too few words for event "
		    "counter in cmvlc data.");
		/* log_dump(LOGL, dest, event_len * sizeof (uint32_t)); */
		result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
		goto done;
	}

	v1725->module.event_counter.value =
	    a_in_buffer[0] | (a_in_buffer[1] << 16);

	if (rand() < 2000)
	  v1725->module.event_counter.value += 20;

	*a_in_used = 2;

	/*
	if (v1725->module.event_counter.value % 100000 == 0) {
		LOGF(info)(LOGL, "rm: %d  us: %d  cnt: %d\n",
		    a_in_remain, *a_in_used,
		    v1725->module.event_counter.value);
	}
	*/

done:
	return result;
#else
	(void) a_module;
	(void) a_in_buffer;
	(void) a_in_remain;
	(void) a_in_used;
	return 0; /* Should not be used, better return some error code? */
#endif
}

uint32_t
caen_v1725_cmvlc_fetch(struct Crate *a_crate,
    struct Module *a_module, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
#if NCONF_mMAP_bCMVLC
	struct CaenV1725Module *v1725;
	uint32_t *outp;
	uint32_t result;

	size_t used;
	size_t block_len;

	/* uint32_t agg_header_len; */

	int ret;

	(void) a_crate;
	(void) v1725;

	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

	outp = a_event_buffer->ptr;
        result = 0;

	ret = cmvlc_block_get(g_cmvlc, a_in_buffer, a_in_remain, &used,
	    outp, 0x10000 /* 0x8000 MBLT words*/, &block_len);

	if (ret < 0) {
		log_error(LOGL, "CAEN V1725: Failed to get cmvlc block: "
		    "%d.", ret);
		/* log_dump(LOGL, dest, event_len * sizeof (uint32_t)); */
		result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
		goto done;
	}

	if (block_len > 0)
	  {
	    /* Discard filler word at the end om MBLT transfer. */
	    /* TODO: is this safe, or do we need to traverse the
	     * aggregates.  It is not enough to check for an initial
	     * aggregate header, there may be several in one block
	     * transfer.
	     */
	    if (outp[block_len-1] == 0xffffffff)
	      block_len--;
	  }

	*a_in_used = (uint32_t) used;
	outp += block_len;

done:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	return result;
#else
	(void) a_crate;
	(void) a_module;
	(void) a_event_buffer;
	(void) a_in_buffer;
	(void) a_in_remain;
	(void) a_in_used;
	return 0; /* Should not be used, better return some error code? */
#endif
}

uint32_t
caen_v1725_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV1725Module *v1725;
	uint32_t *p32;
	uint32_t i, event_size;
	uint32_t result;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");
	result = 0;
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);

	p32 = a_event_buffer->ptr;

	event_size = MAP_READ(v1725->sicy_map, event_size);
	LOGF(spam)(LOGL, "Event_size = %u.", event_size);
	if (0) if (0 == event_size) {
		log_error(LOGL, "Event buffer empty!");
		result = CRATE_READOUT_FAIL_DATA_MISSING;
		goto caen_v1725_readout_fail;
	}

	/* Move data from module. */
	if (KW_NOBLT == v1725->blt_mode) {
		for (i = 0; i < event_size; ++i) {
			uint32_t u32;

			u32 = MAP_READ(v1725->sicy_map, event_readout_buffer);
			*p32++ = u32;
		}
	} else {
		size_t bytes;
		int ret;

		bytes = event_size * sizeof(uint32_t);
		p32 = map_align(p32, &bytes, v1725->blt_mode, DMA_FILLER);
		ret = map_blt_read_berr(v1725->dma_map, 0, p32, bytes);
		if (-1 == ret) {
			log_error(LOGL, "DMA read failed!");
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto caen_v1725_readout_fail;
		} else if (bytes == (size_t) ret) {
			/* Got all data that was promised. */
			p32 += bytes / sizeof(uint32_t);
		} else if (0 == ret) {
			/* The V1725 sometimes lies about event_size.
			 * Looks like it has the previous event_size, but
			 * there is no new data available.
			 * We accept and treat that as no data available.
			 */
			/* printf ("0 read from v1725.\n"); */
		} else {
			/* Short read.  Not expected. */
			log_error(LOGL, "Partial DMA read - unexpected!");
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto caen_v1725_readout_fail;
		}
	}

caen_v1725_readout_fail:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p32);
	LOGF(spam)(LOGL, NAME" readout }");
	return result;
}

uint32_t
caen_v1725_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV1725Module *v1725;
	uint32_t event_size;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_CAEN_V1725, v1725, a_module);
	event_size = MAP_READ(v1725->sicy_map, event_size);
	LOGF(spam)(LOGL, NAME" readout_dt(event_size=%u) }", event_size);
	return 0;
}

void
caen_v1725_setup_(void)
{
	MODULE_SETUP(caen_v1725, 0);
	MODULE_CALLBACK_BIND(caen_v1725, cmvlc_init);
	MODULE_CALLBACK_BIND(caen_v1725, cmvlc_fetch_dt);
	MODULE_CALLBACK_BIND(caen_v1725, cmvlc_fetch);
}

void
set_thresholds(struct CaenV1725Module *a_v1725, uint32_t const
    *a_threshold_array, size_t a_threshold_num)
{
	size_t i;

	LOGF(verbose)(LOGL, NAME" set_thresholds {");
	assert(16 == a_threshold_num);
	for (i = 0; a_threshold_num > i; ++i) {
		uint32_t u32;

		u32 = a_threshold_array[i];
		LOGF(verbose)(LOGL, " [%"PRIz"]=0x%08x.", i, u32);
		MAP_WRITE(a_v1725->sicy_map, trigger_threshold(i),
		    u32);
		(void) a_v1725;
	}
	LOGF(verbose)(LOGL, NAME" set_thresholds }");
}
