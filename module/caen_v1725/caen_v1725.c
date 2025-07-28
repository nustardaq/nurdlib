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

#define NAME "Caen v1725"

#define DMA_FILLER 0x17251725
#define NO_DATA_TIMEOUT 1.0

#define BOARD_CFG_AUTOMATIC_FLUSH      (1 << 0)
#define BOARD_CFG_PROPAGATE_TRIGGER    (1 << 2)
#define BOARD_CFG_DUAL_TRACE           (1 << 11)
#define BOARD_CFG_WAVEVFORM_RECORDING  (1 << 16)
#define BOARD_CFG_EXTRAS_RECORDING     (1 << 17)
#define BOARD_CFG_RESERVED_FIXED_1     0x000c0110

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
	    cfg, CONFIG_UNIT_MV, 0, 2000.0);\
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
			MAP_WRITE(v1725->sicy_map, input_dynamic_range(i),
			    u32);
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
			  (fraction - 1) << 8 |
			  (cfd_width[i] - 1) << 10;
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
			MAP_WRITE(v1725->sicy_map, dc_offset(i), u32);
		}
		time_sleep(init_sleep);
	}
	{
		enum Keyword c_kw_clock[] = {KW_INTERNAL, KW_EXTERNAL};
		uint32_t acq;

		acq =
		    ACQ_MODE_SW |
		    /* ACQ_START | */
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
		MAP_WRITE(v1725->sicy_map, acquisition_control, acq);
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
		MAP_WRITE(v1725->sicy_map, global_trigger_mask, u32);
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
		MAP_WRITE(v1725->sicy_map, front_panel_i_o_control, u32);
	}
	{
		v1725->channel_enable = config_get_bitmask(
		    v1725->module.config, KW_CHANNEL_ENABLE, 0, 15);
		LOGF(verbose)(LOGL, "Channel mask=0x%08x.",
		    v1725->channel_enable);
		MAP_WRITE(v1725->sicy_map, channel_enable_mask,
		    v1725->channel_enable);
	}
	/* DPP Algorithm Control */
	{
		double charge_dbl[16];
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
		    KW_CHARGE, CONFIG_UNIT_FC, 1.25, 5120);
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
		    KW_BASELINE_AVERAGE, CONFIG_UNIT_NONE, 0, 1024);
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
			uint32_t charge;
			uint32_t suppress_charge_zero;
			uint32_t test_pulse_polarity_val;
			uint32_t test_pulse_freq_val;
			uint32_t baseline_average_val;
			uint32_t discrimination_val;
			uint32_t trigger_method_val;

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

			if      (test_pulse_freq[i] <= 500)
				test_pulse_freq_val = 0;
			else if (test_pulse_freq[i] <= 5000)
				test_pulse_freq_val = 1;
			else if (test_pulse_freq[i] <= 50000)
				test_pulse_freq_val = 2;
			else /* (test_pulse_freq[i] <= 500000) */
				test_pulse_freq_val = 3;

			if      (baseline_average[i] == 0)
				baseline_average_val = 0;
			else if (baseline_average[i] <= 16)
				baseline_average_val = 1;
			else if (baseline_average[i] <= 64)
				baseline_average_val = 2;
			else if (baseline_average[i] <= 256)
				baseline_average_val = 3;
			else /* (baseline_average[i] <= 1024) */
				baseline_average_val = 4;

			switch (discrimination[i]) {
			default: /* Avoid uninitialised variable warning. */
			case KW_LED: discrimination_val = 0; break;
			case KW_CFD: discrimination_val = 1; break;
			}

			switch (test_pulse_polarity[i]) {
			default: /* Avoid uninitialised variable warning. */
			case KW_POS: test_pulse_polarity_val = 0; break;
			case KW_NEG: test_pulse_polarity_val = 1; break;
			}

			switch (trigger_method[i]) {
			default: /* Avoid uninitialised variable warning. */
			case KW_INDEPENDENT:     trigger_method_val = 0; break;
			case KW_COINCIDENCE:     trigger_method_val = 1; break;
			case KW_ANTICOINCIDENCE: trigger_method_val = 3; break;
			}

			if (charge_zero_suppression_threshold[i] >= 0)
			  suppress_charge_zero = 1;
			else
			  suppress_charge_zero = 0;

			u32 =
			    charge << 0 |
			    pedestal[i] << 4 |
			    trigout_all[i] << 5 |
			    discrimination_val << 6 |
			    pileup_trigout[i] << 7 |
			    test_pulse[i] << 8 |
			    test_pulse_freq_val << 9 |
			    baseline_restart[i] << 15 |
			    test_pulse_polarity_val << 16 |
			    trigger_method_val << 18 |
			    baseline_average_val << 20 |
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
		uint32_t shaped_self_trigger[8];
		uint32_t pair_trigger_validation[8];
		uint32_t trigger_validation[16];
		uint32_t extra_word[16];
		uint32_t smoothing[16];
		uint32_t trigger_flag_downscale[16];
		uint32_t veto_source[16];
		uint32_t piggy_to_mobo[16];
		uint32_t mark_saturated[16];
		uint32_t veto_direct[16];
		size_t i;

		enum Keyword c_shaped_self_trigger[] = {
			KW_AND,
			KW_CH_EVEN,
			KW_CH_ODD,
			KW_OR,
			KW_OFF
		};
		enum Keyword c_pair_trigger_validation[] = {
			KW_OFF,
			KW_MOBO,
			KW_AND,
			KW_OR
		};
		enum Keyword c_trigger_validation[] = {
			KW_PAIR,
			KW_AND,
			KW_OR
		};
		enum Keyword c_extra_word[] = {
			KW_EXT_TS_BASELINE,
			KW_EXT_TS_FLAGS,
			KW_EXT_TS_FINETIME,
			KW_TRIG_COUNTS,
			KW_ZERO_CROSSING,
			KW_FIXED
		};
		enum Keyword c_veto_source[] = {
			KW_OFF,
			KW_COMMON,
			KW_PAIR,
			KW_SATURATED
		};

		/*
		 * Group-level settings.
		 */
		PREPARE_KEYWORD_CONFIG(shaped_self_trigger,
		    KW_SHAPED_SELF_TRIGGER, c_shaped_self_trigger);
		PREPARE_KEYWORD_CONFIG(pair_trigger_validation,
		    KW_PAIR_TRIGGER_VALIDATION, c_pair_trigger_validation);
		/*
		 * To make things more interesting:
		 * From here, settings are per-channel.
		 */
		PREPARE_KEYWORD_CONFIG(trigger_validation,
		    KW_TRIGGER_VALIDATION, c_trigger_validation);
		PREPARE_KEYWORD_CONFIG(extra_word,
		    KW_EXTRA_WORD, c_extra_word);
		CONFIG_GET_INT_ARRAY(smoothing, v1725->module.config,
		    KW_SMOOTHING, CONFIG_UNIT_NONE, 0, 16);
		CONFIG_GET_INT_ARRAY(trigger_flag_downscale,
		    v1725->module.config,
		    KW_TRIGGER_FLAG_DOWNSCALE, CONFIG_UNIT_NONE, 0, 16);
		/* Should be keyword? */
		CONFIG_GET_INT_ARRAY(piggy_to_mobo, v1725->module.config,
		    KW_PIGGY_TO_MOBO, CONFIG_UNIT_NONE, 0, 12);
		PREPARE_BOOLEAN_CONFIG(mark_saturated,
		    KW_MARK_SATURATED);
		PREPARE_KEYWORD_CONFIG(veto_source,
		    KW_VETO_SOURCE, c_veto_source);
		PREPARE_BOOLEAN_CONFIG(veto_direct,
		    KW_VETO_DIRECT);

		/* Missing: Reset timestamp on veto. */
		/* Resetting time we try to avoid! */

		/* Apply per-channel. */
		for (i = 0; i < LENGTH(trigger_validation); ++i) {
			uint32_t u32;
			uint32_t shaped_trigger_mode = 0;
			uint32_t enable_shaped_trigger = 1;
			uint32_t pair_trigger_validation_mode = 3;
			uint32_t enable_trigger_validation = 1;
			uint32_t ch_trigger_validation_mode = 0;
			uint32_t extra_word_val;
			uint32_t use_smoothing = 1;
			uint32_t smoothing_val = 0;
			uint32_t trigger_flag_downscale_val;
			uint32_t veto_source_val;

			/* Group-level settings, thus [i/2]. */
			switch (shaped_self_trigger[i/2]) {
			case KW_OFF:     enable_shaped_trigger = 0; break;
			case KW_AND:     shaped_trigger_mode = 0; break;
			case KW_CH_EVEN: shaped_trigger_mode = 1; break;
			case KW_CH_ODD:  shaped_trigger_mode = 2; break;
			case KW_OR:      shaped_trigger_mode = 3; break;
			}

			switch (pair_trigger_validation[i/2]) {
			case KW_OFF:  enable_trigger_validation = 0; break;
			case KW_MOBO: pair_trigger_validation_mode = 1; break;
			case KW_AND:  pair_trigger_validation_mode = 2; break;
			case KW_OR:   pair_trigger_validation_mode = 3; break;
			}

			/* Per-channel settings, [i] from here on. */
			switch (trigger_validation[i]) {
			case KW_PAIR: ch_trigger_validation_mode = 0; break;
			case KW_AND:  ch_trigger_validation_mode = 1; break;
			case KW_OR:   ch_trigger_validation_mode = 2; break;
			}

			switch (extra_word[i]) {
			case KW_EXT_TS_BASELINE: extra_word_val = 0; break;
			case KW_EXT_TS_FLAGS:    extra_word_val = 1; break;
			case KW_EXT_TS_FINETIME: extra_word_val = 2; break;
			case KW_TRIG_COUNTS:	 extra_word_val = 4; break;
			case KW_ZERO_CROSSING:	 extra_word_val = 5; break;
			default: /* Avoid uninitialised variable warning. */
			case KW_FIXED:		 extra_word_val = 7; break;
			}

			if      (smoothing[i] == 0)     use_smoothing = 0;
			else if (smoothing[i] <= 2)     smoothing_val = 1;
			else if (smoothing[i] <= 4)     smoothing_val = 2;
			else if (smoothing[i] <= 8)     smoothing_val = 3;
			else /* (smoothing[i] <= 16) */ smoothing_val = 4;

			if      (trigger_flag_downscale[i] <= 128)
			  trigger_flag_downscale_val = 0;
			else if (trigger_flag_downscale[i] <= 1028)
			  trigger_flag_downscale_val = 1;
			else /* (trigger_flag_downscale[i] <= 8192) */
			  trigger_flag_downscale_val = 2;

			switch (veto_source[i]) {
			default: /* Avoid uninitialised variable warning. */
			case KW_OFF:       veto_source_val = 0; break;
			case KW_COMMON:    veto_source_val = 1; break;
			case KW_PAIR:      veto_source_val = 2; break;
			case KW_SATURATED: veto_source_val = 3; break;
			}

			u32 =
			    shaped_trigger_mode << 0 |
			    enable_shaped_trigger << 2 |
			    pair_trigger_validation_mode << 4 |
			    enable_trigger_validation << 6 |
			    extra_word_val << 8 |
			    use_smoothing << 11 |
			    smoothing_val << 12 |
			    trigger_flag_downscale_val << 16 |
			    veto_source_val << 18 |
			    piggy_to_mobo[i] << 20 |
			    mark_saturated[i] << 24 |
			    ch_trigger_validation_mode << 25 |
			    veto_direct[i] << 27 /* |
			    reset_time_on_ext_veto[i] << 28*/;

			LOGF(verbose)(LOGL, "DPP algo ctrl2[%"PRIz"]=0x%08x.",
			    i, u32);
			MAP_WRITE(v1725->sicy_map, dpp_algorithm_control_2(i),
			    u32);
		}
	}
	/* Veto width. */
	{
		uint32_t veto_width[16];
		size_t i;

		/* The maximum value is actually insanely large:
		 * 1725: 264 ms * 0xffff = 17301.504 s = 4.8 h
		 * 1730: 132 ms * 0xffff =  8650.752 s = 2.4 h
		 */

		CONFIG_GET_INT_ARRAY(veto_width, v1725->module.config,
		    KW_VETO_WIDTH, CONFIG_UNIT_NS, 0, 2000000000);

		/* Apply per-channel. */
		for (i = 0; i < LENGTH(veto_width); ++i) {
			uint32_t u32;
			uint32_t veto_width_val;
			uint32_t veto_width_step;

			/* The step sizes are in increments of a factor 256 */
			/* Express as hexadecimal multipliers or the
			 * sampling clock, this is 0x4, 0x400, 0x40000
			 * and 0x4000000.
			 */

			if (veto_width[i] <=
			    0xffff * 0x4 * v1725->period_ns) {
				veto_width_step = 0;
				veto_width_val = veto_width[i]
				    / (0x4 * v1725->period_ns);
			} else if (veto_width[i] <=
			    0xffff * 0x400 * v1725->period_ns) {
				veto_width_step = 1;
				veto_width_val = veto_width[i]
				    / (0x400 * v1725->period_ns);
			} else if (veto_width[i] <=
			    0xffff * (uint64_t) 0x40000 * v1725->period_ns) {
				veto_width_step = 2;
				veto_width_val = veto_width[i]
				    / (0x40000 * v1725->period_ns);
			} else {
				/* This will actually not be used since we
				 * do not handle values above 32 bits of ns.
				 */
				veto_width_step = 3;
				veto_width_val = veto_width[i]
				    / (0x4000000 * v1725->period_ns);
			}

			u32 =
			    veto_width_val << 0 |
			    veto_width_step << 16;

			LOGF(verbose)(LOGL, "Veto width[%"PRIz"]=0x%08x.",
			    i, u32);
			MAP_WRITE(v1725->sicy_map, veto_width(i),
			    u32);
		}
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

	/* TODO: Check geo before reset? */

	/*
	 * Reset, before GEO.
	 * Reset sets board_id to actual backplane geom (slot).
	 */
	MAP_WRITE(v1725->sicy_map, software_reset, 1);
	MAP_WRITE(v1725->sicy_map, software_clear, 1);
	time_sleep(0.1);

	LOGF(verbose)(LOGL, "GEO = %u. (after reset)",
	    0x1f & MAP_READ(v1725->sicy_map, board_id));

	{
		uint32_t form_factor;
		int board_id_set = 0;

		form_factor = MAP_READ(v1725->sicy_map,
		    configuration_rom_board_form_factor);
		/*
		 * Manual states that VME64X has a read-only board_id,
		 * but it can in fact be written on a VX1725S.
		 */
		if (0 == form_factor ||
		    1 == form_factor) {
			/* VME64(x), we can write the nurdlib ID. */
			MAP_WRITE(v1725->sicy_map, board_id, v1725->geo);
			board_id_set = 1;
		}
		v1725->geo = 0x1f & MAP_READ(v1725->sicy_map, board_id);
		LOGF(verbose)(LOGL, "GEO = %u.", v1725->geo);
		if (0 == board_id_set) {
			/* VME64x, we have to remap the nurdlib ID. */
			crate_module_remap_id(a_crate, v1725->module.id,
			    v1725->geo);
		}
	}

	{
		uint32_t u32;

		u32 =
		    BOARD_CFG_AUTOMATIC_FLUSH |
		    BOARD_CFG_PROPAGATE_TRIGGER |
		    BOARD_CFG_EXTRAS_RECORDING |
		    BOARD_CFG_RESERVED_FIXED_1;
		LOGF(verbose)(LOGL, "Board configuration = 0x%08x.", u32);
		MAP_WRITE(v1725->sicy_map, board_configuration_bit_set, u32);
		MAP_WRITE(v1725->sicy_map, board_configuration_bit_clear,
		    ~u32);
	}

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
	MAP_WRITE(v1725->sicy_map, readout_control,
	    (v1725->do_berr ? CTL_BERR_ENABLE : 0) |
	    CTL_ALIGN64 |
	    (v1725->do_blt_ext ? CTL_BLT_EXTENDED : 0));

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
