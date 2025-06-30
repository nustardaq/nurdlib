/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2021, 2023-2025
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

#include <module/gsi_febex/gsi_febex.h>
#include <math.h>
#include <sched.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_febex/internal.h>
#include <module/gsi_pex/offsets.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/memcpy.h>

#define NAME "Gsi Febex"

#define REG_FEB_CTRL          0x200000
#define REG_FEB_TRIG_DELAY    0x200004
#define REG_FEB_TRACE_LEN     0x200008
#define REG_FEB_SELF_TRIG     0x20000C
#define REG_FEB_STEP_SIZE     0x200010
#define REG_FEB_TIME          0x200018
#define ENERGY_SUM_A_REG      0x208090
#define ENERGY_GAP_REG        0x2080A0
#define ENERGY_SUM_B_REG      0x2080B0
#define DATA_FILT_CONTROL_REG 0x2080C0
#define TRIG_SUM_A_REG        0x2080D0
#define TRIG_GAP_REG          0x2080E0
#define TRIG_SUM_B_REG        0x2080F0
#define REG_DATA_REDUCTION    0xFFFFB0
#define REG_MEM_DISABLE       0xFFFFB4

struct FilterSetting {
	int	left_sum;
	int	gap;
	int	right_sum;
};
struct Setting {
	int	polarity;
	int	trigger_method;
	int	do_even_odd;
	int	do_filter_trace;
	struct	FilterSetting trigger_filter;
	int	do_energy_filter;
	struct	FilterSetting energy_filter;
};

void	setting_override(struct Setting *, struct ConfigBlock *);

MODULE_PROTOTYPES(gsi_febex);

/* Puts non-"null" config values into setting struct. */
void
setting_override(struct Setting *a_setting, struct ConfigBlock *a_block)
{
	enum Keyword const c_polarity[] = {KW_POS, KW_NEG, KW_INHERIT};
	double trigger_d;
	int polarity;

#define GET_BOOLEAN(attr, name) do {\
		enum Keyword const c_boolean[] = {KW_TRUE, KW_FALSE,\
		    KW_INHERIT};\
		int boolean;\
		boolean = CONFIG_GET_KEYWORD(a_block, name, c_boolean);\
		if (KW_TRUE == boolean) {\
			a_setting->attr = 1;\
		} else if (KW_FALSE == boolean) {\
			a_setting->attr = 0;\
		}\
	} while (0)

	polarity = CONFIG_GET_KEYWORD(a_block, KW_SIGNAL_POLARITY,
	    c_polarity);
	if (KW_POS == polarity) {
		a_setting->polarity = 0;
	} else if (KW_NEG == polarity) {
		a_setting->polarity = 1;
	}
	trigger_d = config_get_double(a_block, KW_TRIGGER_METHOD,
	    CONFIG_UNIT_NONE, 2, 61);
	if (3 == trigger_d) {
		a_setting->trigger_method = 0;
	} else if (60 == trigger_d) {
		a_setting->trigger_method = 1;
	} else if (30 == trigger_d) {
		a_setting->trigger_method = 2;
	} else if (15 == trigger_d) {
		a_setting->trigger_method = 3;
	} else if (7.5 == trigger_d) {
		a_setting->trigger_method = 4;
	}
	GET_BOOLEAN(do_even_odd, KW_EVEN_ODD);
	GET_BOOLEAN(do_filter_trace, KW_FILTER_TRACE);

#define GET_FILTER(type, attr, TYPE, ATTR) do {\
		int i;\
		i = config_get_int32(a_block, KW_##TYPE##_FILTER_##ATTR,\
		    CONFIG_UNIT_NONE, 0, 10);\
		if (0 != i) {\
			a_setting->type##_filter.attr = i;\
		}\
	} while (0)
	GET_FILTER(trigger, left_sum, TRIGGER, LEFT_SUM);
	GET_FILTER(trigger, gap, TRIGGER, GAP);
	GET_FILTER(trigger, right_sum, TRIGGER, RIGHT_SUM);

	GET_BOOLEAN(do_energy_filter, KW_ENERGY_FILTER);
	GET_FILTER(energy, left_sum, ENERGY, LEFT_SUM);
	GET_FILTER(energy, gap, ENERGY, GAP);
	GET_FILTER(energy, right_sum, ENERGY, RIGHT_SUM);
}

uint32_t
gsi_febex_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
gsi_febex_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct ConfigBlock *card_block;
	struct GsiFebexModule *feb;
	size_t card_i;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(feb);
	feb->module.event_max = 1;
	feb->module.event_counter.mask = 0;
	feb->sfp_i = config_get_block_param_int32(a_block, 0);
	if (feb->sfp_i > 3) {
		log_die(LOGL, NAME" create: SFP=%"PRIz" != [0,3] does not "
		    "seem right.", feb->sfp_i);
	}

	/* Count cards. */
	for (card_block = config_get_block(a_block, KW_GSI_FEBEX_CARD);
	    NULL != card_block;
	    card_block = config_get_block_next(card_block, KW_GSI_FEBEX_CARD))
	{
		card_i = config_get_block_param_int32(card_block, 0);
		feb->card_num = MAX(feb->card_num, card_i + 1);
	}
	if (!IN_RANGE_II(feb->card_num, 1, 256)) {
		log_die(LOGL, NAME" create: Card-num=%"PRIz" != [1,256], "
		    "please add some Febex cards in your config.",
		    feb->card_num);
	}
	/* Store config block for each card. */
	CALLOC(feb->card_array, feb->card_num);
	for (card_block = config_get_block(a_block, KW_GSI_FEBEX_CARD);
	    NULL != card_block;
	    card_block = config_get_block_next(card_block, KW_GSI_FEBEX_CARD))
	{
		card_i = config_get_block_param_int32(card_block, 0);
		feb->card_array[card_i].config = card_block;
	}

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)feb;
}

void
gsi_febex_deinit(struct Module *a_module)
{
	(void)a_module;
}

void
gsi_febex_destroy(struct Module *a_module)
{
	struct GsiFebexModule *feb;

	LOGF(verbose)(LOGL, NAME" destroy {");
	MODULE_CAST(KW_GSI_FEBEX, feb, a_module);
	FREE(feb->card_array);
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
gsi_febex_get_map(struct Module *a_module)
{
	(void)a_module;
	return NULL;
}

void
gsi_febex_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

#define FEBEX_INIT_RD(a0, a1, label) do {\
	if (!gsi_pex_slave_read(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%"PRIz":card=%"PRIz" read("#a0\
		    ", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)
#define FEBEX_INIT_WR(a0, a1, label) do {\
	if (!gsi_pex_slave_write(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%"PRIz":card=%"PRIz" write("#a0\
		    ", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)

int
gsi_febex_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct Setting setting;
	struct ModuleGate gate;
	struct GsiFebexModule *feb;
	struct GsiPex *pex;
	size_t card_i, card_num, sfp_i;
	unsigned clock_input_card_i;
	int ret, trigger_delay;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");
	ret = 0;
	MODULE_CAST(KW_GSI_FEBEX, feb, a_module);
	pex = crate_gsi_pex_get(a_crate);

	/*
	 * Global defaults.
	 */

	module_gate_get(&gate, a_module->config, -0x7ff * 5, 0, 0, 0x7ff * 5);
	if (0 < gate.time_after_trigger_ns ||
	    gate.time_after_trigger_ns + gate.width_ns < 0) {
		log_die(LOGL, NAME" Invalid gate %f..%f, must span 0.",
		    gate.time_after_trigger_ns, gate.time_after_trigger_ns +
		    gate.width_ns);
	}
	/* Cannot do odd number of samples in a trace. */
	feb->trace_length = (unsigned)ceil(gate.width_ns / 10) * 2;
	trigger_delay = floor(-gate.time_after_trigger_ns);
	LOGF(verbose)(LOGL, "Slices=%d Trigger-delay=%d.", feb->trace_length,
	    trigger_delay);

	clock_input_card_i = config_get_int32(a_module->config,
	    KW_CLOCK_INPUT, CONFIG_UNIT_NONE, 0, feb->card_num - 1);

	/*
	 * Per-card defaults.
	 */

	/* Put borked default values to find errors later. */
	setting.polarity = -1;
	setting.trigger_method = -1;
	setting.do_even_odd = -1;
	setting.do_filter_trace = -1;
	setting.trigger_filter.left_sum = -1;
	setting.trigger_filter.gap = -1;
	setting.trigger_filter.right_sum = -1;
	setting.do_energy_filter = -1;
	setting.energy_filter.left_sum = -1;
	setting.energy_filter.gap = -1;
	setting.energy_filter.right_sum = -1;
	setting_override(&setting, a_module->config);

	sfp_i = feb->sfp_i;
	card_num = feb->card_num;
	LOGF(verbose)(LOGL, "febex_init_fast(SFP=%"PRIz", cards=%"PRIz") {",
	    sfp_i, card_num);
	for (card_i = 0; card_i < card_num; ++card_i) {
		struct Setting card_setting;
		uint16_t threshold_array[16];
		struct FilterSetting const *filter;
		struct ConfigBlock *card_block;
		uint32_t channel_disable, zero_suppress_mask, do_self_trigger,
			 self_trig;
		size_t ch_i;

		LOGF(verbose)(LOGL, "Card=%"PRIz"<%"PRIz"", card_i, card_num);
		card_block = feb->card_array[card_i].config;

		memcpy_(&card_setting, &setting, sizeof card_setting);
		setting_override(&card_setting, card_block);

		/*
		 * Nothing beats reversing the norm, fighting mainstream,
		 * going your own way...
		 */
		channel_disable = 0xffff & ~config_get_bitmask(card_block,
		    KW_CHANNEL_ENABLE, 0, 15);
		zero_suppress_mask = config_get_bitmask(card_block,
		    KW_ZERO_SUPPRESS, 0, 15);
		do_self_trigger = 0xffff & ~config_get_bitmask(card_block,
		    KW_USE_EXTERNAL_TRIGGER, 0, 15);
		self_trig = card_setting.do_even_odd << 21 |
		    card_setting.polarity << 20 | card_setting.trigger_method
		    << 16 | do_self_trigger;

		CONFIG_GET_INT_ARRAY(threshold_array, card_block,
		    KW_THRESHOLD, CONFIG_UNIT_NONE, 0, 0x3fff);

		FEBEX_INIT_WR(REG_FEB_TRACE_LEN, feb->trace_length,
		    febex_init_fast_done);
		FEBEX_INIT_WR(REG_FEB_TRIG_DELAY, trigger_delay,
		    febex_init_fast_done);

		FEBEX_INIT_WR(REG_FEB_SELF_TRIG, self_trig,
		    febex_init_fast_done);
		for (ch_i = 0; ch_i < 16; ++ch_i) {
			FEBEX_INIT_WR(REG_FEB_STEP_SIZE,
			    ch_i << 24 | threshold_array[ch_i],
			    febex_init_fast_done);
		}

		if (clock_input_card_i == card_i) {
			FEBEX_INIT_WR(REG_FEB_TIME, 0, febex_init_fast_done);
			FEBEX_INIT_WR(REG_FEB_TIME, 7, febex_init_fast_done);
		} else {
			FEBEX_INIT_WR(REG_FEB_TIME, 0, febex_init_fast_done);
			FEBEX_INIT_WR(REG_FEB_TIME, 5, febex_init_fast_done);
		}

		FEBEX_INIT_WR(REG_DATA_REDUCTION, zero_suppress_mask << 1,
		    febex_init_fast_done);
		FEBEX_INIT_WR(REG_MEM_DISABLE, channel_disable << 1,
		    febex_init_fast_done);

		filter = &card_setting.trigger_filter;
		FEBEX_INIT_WR(TRIG_SUM_A_REG,
		    filter->left_sum,
		    febex_init_fast_done);
		FEBEX_INIT_WR(TRIG_GAP_REG,
		    filter->left_sum + filter->gap,
		    febex_init_fast_done);
		FEBEX_INIT_WR(TRIG_SUM_B_REG,
		    filter->left_sum + filter->gap + filter->right_sum,
		    febex_init_fast_done);

		if (card_setting.do_energy_filter) {
			filter = &card_setting.energy_filter;
			FEBEX_INIT_WR(ENERGY_SUM_A_REG,
			    filter->left_sum,
			    febex_init_fast_done);
			FEBEX_INIT_WR(ENERGY_GAP_REG,
			    filter->left_sum + filter->gap,
			    febex_init_fast_done);
			FEBEX_INIT_WR(ENERGY_SUM_B_REG,
			    filter->left_sum + filter->gap + filter->right_sum,
			    febex_init_fast_done);
		}
		feb->card_array[card_i].do_filter_trace =
		    card_setting.do_filter_trace;
	}
	/* Check clocks. */
	for (card_i = 0; card_i < card_num; ++card_i) {
		uint32_t feb_time;

		FEBEX_INIT_RD(REG_FEB_TIME, &feb_time, febex_init_fast_done);
		if (0x10 != (0x10 & feb_time)) {
			log_error(LOGL, NAME" SFP=%"PRIz":card=%"PRIz" Clock"
			    " missing.", sfp_i, card_i);
			goto febex_init_fast_done;
		}
	}
	/* Run! */
	for (card_i = 0; card_i < card_num; ++card_i) {
		FEBEX_INIT_WR(DATA_FILT_CONTROL_REG, 0x80 |
		    (feb->card_array[card_i].do_filter_trace << 1),
		    febex_init_fast_done);
	}
	ret = 1;
febex_init_fast_done:
	LOGF(info)(LOGL, NAME" init_fast }");
	return ret;
}

int
gsi_febex_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiFebexModule *feb;
	struct GsiPex *pex;
	size_t card_i, card_num, sfp_i;
	int ret;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_slow {");
	ret = 0;
	MODULE_CAST(KW_GSI_FEBEX, feb, a_module);
	pex = crate_gsi_pex_get(a_crate);

	sfp_i = feb->sfp_i;
	card_num = feb->card_num;
	LOGF(verbose)(LOGL, "febex_init_slow(SFP=%"PRIz", cards=%"PRIz") {",
	    sfp_i, card_num);
	gsi_pex_sfp_tag(pex, sfp_i);
	if (!gsi_pex_slave_init(pex, sfp_i, card_num)) {
		log_error(LOGL,
		    "SFP=%"PRIz":cards=%"PRIz" slave init failed.", sfp_i,
		    card_num);
		goto febex_init_slow_done;
	}
	for (card_i = 0; card_i < card_num; ++card_i) {
		uint32_t reg_feb_ctrl;

		LOGF(verbose)(LOGL, "Card=%"PRIz"<%"PRIz"", card_i, card_num);

		/* Reset. */
		FEBEX_INIT_WR(DATA_FILT_CONTROL_REG, 0x00,
		    febex_init_slow_done);
		/* Disable test data length. */
		FEBEX_INIT_WR(REG_DATA_LEN, 0x10000000, febex_init_slow_done);

		/* Disable trigger accept. */
		FEBEX_INIT_WR(REG_FEB_CTRL, 0, febex_init_slow_done);
		FEBEX_INIT_RD(REG_FEB_CTRL, &reg_feb_ctrl,
		    febex_init_slow_done);
		if (0 != (1 & reg_feb_ctrl)) {
			log_error(LOGL, NAME" SFP=%"PRIz":card=%"PRIz" Could"
			    " not set trigger acceptance.", sfp_i, card_i);
			goto febex_init_slow_done;
		}

		/* Enable trigger accept. */
		FEBEX_INIT_WR(REG_FEB_CTRL, 1, febex_init_slow_done);
		FEBEX_INIT_RD(REG_FEB_CTRL, &reg_feb_ctrl,
		    febex_init_slow_done);
		if (1 != (1 & reg_feb_ctrl)) {
			log_error(LOGL, NAME" SFP=%"PRIz":card=%"PRIz" Could"
			    " not set trigger acceptance.", sfp_i, card_i);
			goto febex_init_slow_done;
		}

		FEBEX_INIT_WR(REG_HEADER, sfp_i, febex_init_slow_done);
	}
	ret = 1;
febex_init_slow_done:
	LOGF(info)(LOGL, NAME" init_slow }");
	return ret;
}

void
gsi_febex_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_febex_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct GsiFebexModule *feb;
	uint32_t const *p32, *end;
	unsigned gsi_mbs_trigger, sfp_i;
	int ret;

	(void)a_do_pedestals;
	ret = 0;
	LOGF(spam)(LOGL, NAME" parse {");
	gsi_mbs_trigger = crate_gsi_mbs_trigger_get(a_crate);
	MODULE_CAST(KW_GSI_FEBEX, feb, a_module);
	sfp_i = feb->sfp_i;
	p32 = a_event_buffer->ptr;
	ASSERT(size_t, PRIz, 0, ==, 3 & a_event_buffer->bytes);
	end = p32 + a_event_buffer->bytes / 4;
	for (;;) {
		uint32_t u32;
		unsigned card_i, ch_i, footer_marker;

		if (p32 > end) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Parsed beyond limit, confused.");
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_febex_parse_data_done;
		}
		if (p32 == end) {
			break;
		}
		/* Must have at least 4 32-bitters. */
		if (!MEMORY_CHECK(*a_event_buffer, &p32[4])) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Missing data.");
			ret |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_febex_parse_data_done;
		}
		/* TDC header. */
		u32 = *p32;
		if (0x34 != (u32 & 0x000000ff)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header does not have 0x34.");
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_febex_parse_data_done;
		}
		if (gsi_mbs_trigger != ((u32 & 0x00000f00) >> 8)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header does not have TRIVA trigger %d.",
			    gsi_mbs_trigger);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_febex_parse_data_done;
		}
		if (sfp_i != ((u32 & 0x0000f000) >> 16)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header does not have SFP ID %d.", sfp_i);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_febex_parse_data_done;
		}
		card_i = (u32 & 0x00ff0000) >> 16;
		if (feb->card_num <= card_i) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header has invalid card ID %d >= num "
			    "%"PRIz".", card_i, feb->card_num);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_febex_parse_data_done;
		}
		ch_i = (u32 & 0xff000000) >> 24;
		if (0xff == ch_i) {
			uint32_t bytes;

			/* No trace data. */
			++p32;
			bytes = *p32;
			/* Check memory according to 'bytes'. */
			if (!MEMORY_CHECK(*a_event_buffer, &p32[bytes / 4])) {
				module_parse_error(LOGL, a_event_buffer,
				    p32, "TDC no-trace data smaller than "
				    "reported.");
				ret |= CRATE_READOUT_FAIL_DATA_MISSING;
				goto gsi_febex_parse_data_done;
			}
			/* Second TDC u32. */
			++p32;
			u32 = *p32;
			if (0xaf != (u32 >> 24)) {
				module_parse_error(LOGL, a_event_buffer,
				    p32, "TDC second header does not have "
				    "0xaf.");
				ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto gsi_febex_parse_data_done;
			}
			/*
			 * Skip header + trigger-time + time + data.
			 */
			p32 += bytes / 4 - 1;
			footer_marker = 0xbf;
		} else if (16 > ch_i) {
			struct GsiFebexModuleCard const *card;
			uint32_t bytes;

			card = &feb->card_array[card_i];
			/* Trace data. */
			++p32;
			bytes = *p32;
			/* Check memory according to 'bytes'. */
			if (!MEMORY_CHECK(*a_event_buffer, &p32[bytes /
			    4])) {
				module_parse_error(LOGL, a_event_buffer,
				    p32, "TDC trace data less than "
				    "reported.");
				ret |= CRATE_READOUT_FAIL_DATA_MISSING;
				goto gsi_febex_parse_data_done;
			}
			if (bytes / 4 - 2 !=
			    (card->do_filter_trace ?
			    feb->trace_length * 2 :
			    feb->trace_length / 2)) {
				module_parse_error(LOGL, a_event_buffer,
				    p32, "TDC data too small for configured "
				    "trace length.");
				ret |= CRATE_READOUT_FAIL_DATA_MISSING;
				goto gsi_febex_parse_data_done;
			}
			/* Second TDC u32. */
			++p32;
			u32 = *p32;
			if (0xaa != (u32 >> 24)) {
				module_parse_error(LOGL, a_event_buffer,
				    p32, "TDC second header does not have "
				    "0xaa.");
				ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto gsi_febex_parse_data_done;
			}
			/* Skip data based on trace length config. */
			p32 += 1 + (card->do_filter_trace ?
			    feb->trace_length * 2 :
			    feb->trace_length / 2);
			footer_marker = 0xbb;
		} else {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header has invalid channel 0x%02x.", ch_i);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_febex_parse_data_done;
		}
		/* TDC footer. */
		u32 = *p32;
		if (footer_marker != (u32 >> 24)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC footer does not have '0x%x'.",
			    footer_marker);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_febex_parse_data_done;
		}
		++p32;
	}
gsi_febex_parse_data_done:
	LOGF(spam)(LOGL, NAME" parse(0x%08x) }", ret);
	return ret;
}

uint32_t
gsi_febex_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct EventBuffer eb;
	struct GsiPex *pex;
	struct GsiFebexModule *feb;
	struct GsiFebexCrate *crate;
	uintptr_t dst_bursted, phys_minus_virt, burst_mask;
	uint32_t bytes, bytes_bursted;
	unsigned burst, trial;
	int ret;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout {");
	ret = 0;

	pex = crate_gsi_pex_get(a_crate);
	crate = crate_get_febex_crate(a_crate);
	MODULE_CAST(KW_GSI_FEBEX, feb, a_module);

	if (pex->is_parallel) {
/* TODO: Why can we not test the slave #? */
		(void)crate;
		if (!gsi_pex_token_receive(pex, feb->sfp_i,
		    /*crate->sfp[feb->sfp_i]->card_num*/0)) {
			log_error(LOGL, NAME":SFP=%"PRIz": Failed to receive"
			    " token.", feb->sfp_i);
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_febex_readout_done;
		}
	}

	/*
	 * Let's grab the data, folks!
	 * Shouldn't have to add a uint32_t to get the footer, but whatevs...
	 */
	bytes = GSI_PEX_READ(pex, tk_mem_sizen(feb->sfp_i)) +
	    sizeof(uint32_t);
	if (bytes > (2000 * 2 * 4 + 4 * 4) * 16 *
	    crate->sfp[feb->sfp_i]->card_num) {
		log_error(LOGL, NAME":SFP=%"PRIz": Crazy large data size "
		    "0x%08x.", feb->sfp_i, bytes);
		ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
		goto gsi_febex_readout_done;
	}
	if (0xa0 > bytes) {
		burst = 0x10;
	} else if (0x140 > bytes) {
		burst = 0x20;
	} else if (0x280 > bytes) {
		burst = 0x40;
	} else {
		burst = 0x80;
	}
	LOGF(spam)(LOGL, "bytes=0x%08x burst=0x%08x.", bytes, burst);
	/* Adjust buffer to 'burst' boundary. */
	burst_mask = burst - 1;
	COPY(eb, *a_event_buffer);
	gsi_pex_buf_get(pex, &eb, &phys_minus_virt);
	dst_bursted = ((uintptr_t)eb.ptr + burst_mask) & ~burst_mask;
	bytes_bursted = (bytes + burst_mask) & ~burst_mask;
	if (bytes_bursted > a_event_buffer->bytes) {
		log_error(LOGL, NAME":SFP=%"PRIz": Wanted to read %u B, "
		    "but only %"PRIz"B available.", feb->sfp_i,
		    bytes_bursted, a_event_buffer->bytes);
		ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto gsi_febex_readout_done;
	}
	/* Setup DMA, and do it! */
	LOGF(spam)(LOGL,
	    "DMA src=%p dst=%p->%p size=0x%08x->0x%08x burst=0x%08x.",
	    (void *)pex->sfp[feb->sfp_i].phys,
	    eb.ptr,
	    (void *)dst_bursted,
	    bytes,
	    bytes_bursted,
	    burst);
	pex->dma->src = pex->sfp[feb->sfp_i].phys;
	pex->dma->dst = phys_minus_virt + dst_bursted;
	pex->dma->transport_size = bytes_bursted;
	pex->dma->burst_size = burst;
	pex->dma->stat = 1;
	for (trial = 0;;) {
		uint32_t stat;

		stat = pex->dma->stat;
		if (0 == stat) {
			break;
		}
		if (0xffffffff == stat) {
			log_error(LOGL, "PCIe bus error.");
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_febex_readout_done;
		}
		++trial;
		if (0 == trial % 1000000) {
			log_error(LOGL, "DMA not finished, trial=%d (PC may "
			    "need power to be physically pulled).", trial);
		}
		sched_yield();
	}
	/* Move back data to cover DMA address fudging. */
	memmove(a_event_buffer->ptr, (void *)dst_bursted, bytes);
	a_event_buffer->ptr = (uint8_t *)a_event_buffer->ptr + bytes;
gsi_febex_readout_done:
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", ret);
	return ret;
}

uint32_t
gsi_febex_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x) }",
	    a_module->event_counter.value);
	return 0;
}

void
gsi_febex_setup_(void)
{
	MODULE_SETUP(gsi_febex, MODULE_FLAG_EARLY_DT);
}

void
gsi_febex_crate_add(struct GsiFebexCrate *a_crate, struct Module *a_module)
{
	struct GsiFebexModule *feb;
	size_t sfp_i;

	LOGF(verbose)(LOGL, NAME" crate_add {");
	MODULE_CAST(KW_GSI_FEBEX, feb, a_module);
	sfp_i = feb->sfp_i;
	if (NULL != a_crate->sfp[sfp_i]) {
		log_die(LOGL, "Multiple FEBEX chains on SFP=%"PRIz"!",
		    sfp_i);
	}
	a_crate->sfp[sfp_i] = feb;
	LOGF(verbose)(LOGL, NAME" crate_add }");
}

void
gsi_febex_crate_create(struct GsiFebexCrate *a_crate)
{
	ZERO(*a_crate);
}

int
gsi_febex_crate_init_slow(struct GsiFebexCrate *a_crate)
{
	size_t i;

	for (i = 0; i < LENGTH(a_crate->sfp); ++i) {
		struct GsiFebexModule *feb;

		feb = a_crate->sfp[i];
		if (NULL != feb) {
			feb->module.event_counter.value = 0;
		}
	}
	return 1;
}
