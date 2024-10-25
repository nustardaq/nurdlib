/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2021, 2023-2024
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

#include <module/gsi_tamex/gsi_tamex.h>
#include <sched.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_tamex/internal.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/time.h>

#define NAME "Gsi Tamex"

#if !NCONF_mGSI_PEX_bNO
#	include <math.h>
#	include <module/map/map.h>
#	include <module/gsi_pex/offsets.h>
#	include <nurdlib/config.h>
#	include <nurdlib/crate.h>
#	include <nurdlib/log.h>

#	define CLK_SRC_EXT_TAM2  0x20
#	define CLK_SRC_ONB_TAM2  0x24
#	define CLK_SRC_BPL_TAM3  0x26
#	define CLK_SRC_ONB_TAM3  0x2a
#	define CLK_SRC_BPL_TAMP1 0x0
#	define CLK_SRC_ONB_TAMP1 0x1 /* TODO: 4: 1st card = clk master. */
#	define CLK_SRC_EXT_TAMP1 0x2 /* TODO: 8: 1st card = clk receiver. */
#	define REG_TAM_CTRL      0x200000
#	define REG_TAM_TRG_WIN   0x200004
#	define REG_TAM_EN_CH     0x200008
#	define REG_TAM_POLARITY  0x200010 /* TODO: 0x330018 tam3/4 */
#	define REG_TAM_MISC1     0x330010 /* new for test pulse tamex4 */
#	define REG_TAM_MISC2     0x330014 /* new for test pulse tamex4 */
#	define REG_TAM_CLK_SEL   0x311000
#	define REG_TAM_PADI_CTL  0x311014
#	define REG_TAM_PADI_DAT  0x311018
#	define REG_TAM_TRG_EN    0x33001c

MODULE_PROTOTYPES(gsi_tamex);
static void			gate_get(struct ModuleGate *, struct
    ModuleGate const *, struct ConfigBlock *, int);
static struct ConfigBlock	*gsi_tamex_get_submodule_config(struct Module
    *, unsigned);
static void			gsi_tamex_sub_module_pack(struct Module *,
    struct PackerList *);
static int			padi_set_threshold(struct GsiPex *, unsigned,
    unsigned, uint16_t const *, int) FUNC_RETURNS;

void
gate_get(struct ModuleGate *a_gate, struct ModuleGate const *a_parent_gate,
    struct ConfigBlock *a_config, int a_do_long_range)
{
	int mask;

	mask = a_do_long_range ? 0xffff : 0x7ff;
	module_gate_get(a_gate, a_config, -mask * 5, 1, 0, mask * 5);
	if (0 <= a_gate->time_after_trigger_ns ||
	    a_gate->time_after_trigger_ns + a_gate->width_ns <= 0) {
		if (NULL == a_parent_gate) {
			log_die(LOGL, NAME
			    " Invalid gate %f..%f, must span 0.",
			    a_gate->time_after_trigger_ns,
			    a_gate->time_after_trigger_ns + a_gate->width_ns);
		}
		COPY(*a_gate, *a_parent_gate);
	}
}

uint32_t
gsi_tamex_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
gsi_tamex_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	const enum Keyword c_model[] = {KW_TAMEX2, KW_TAMEX2_PADI,
	    KW_TAMEX_PADI1, KW_TAMEX3, KW_TAMEX4_PADI};
	struct GsiTamexModule *tam;
	struct ConfigBlock *card_block;
	size_t card_i;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(tam);
	tam->module.event_max = 1;
	tam->sfp_i = config_get_block_param_int32(a_block, 0);
	if (tam->sfp_i > 4) {
		log_die(LOGL, NAME" create: SFP=%"PRIz" != [0,3] does not "
		    "seem right.", tam->sfp_i);
	}
	tam->model = CONFIG_GET_BLOCK_PARAM_KEYWORD(a_block, 1, c_model);
	/* Count cards. */
	for (card_block = config_get_block(a_block, KW_GSI_TAMEX_CARD);
	    NULL != card_block;
	    card_block = config_get_block_next(card_block, KW_GSI_TAMEX_CARD))
	{
		card_i = config_get_block_param_int32(card_block, 0);
		tam->card_num = MAX(tam->card_num, card_i + 1);
	}
	if (!IN_RANGE_II(tam->card_num, 1, 256)) {
		log_die(LOGL, NAME" create: Card-num=%"PRIz" != [1,256], "
		    "please add some TAMEX cards in your config.",
		    tam->card_num);
	}
	CALLOC(tam->card_array, tam->card_num);
	for (card_block = config_get_block(a_block, KW_GSI_TAMEX_CARD);
	    NULL != card_block;
	    card_block = config_get_block_next(card_block, KW_GSI_TAMEX_CARD))
	{
		card_i = config_get_block_param_int32(card_block, 0);
		tam->card_array[card_i].config = card_block;
	}

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)tam;
}

void
gsi_tamex_deinit(struct Module *a_module)
{
	(void)a_module;
}

void
gsi_tamex_destroy(struct Module *a_module)
{
	struct GsiTamexModule *tam;

	LOGF(verbose)(LOGL, NAME" destroy {");
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	FREE(tam->card_array);
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
gsi_tamex_get_map(struct Module *a_module)
{
	(void)a_module;
	return NULL;
}

void
gsi_tamex_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

struct ConfigBlock *
gsi_tamex_get_submodule_config(struct Module *a_module, unsigned a_i)
{
	struct GsiTamexModule *tam;
	struct ConfigBlock *config;

	LOGF(debug)(LOGL, NAME" get_submodule_config(%u) {", a_i);
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	config = a_i < tam->card_num ? tam->card_array[a_i].config : NULL;
	LOGF(debug)(LOGL, NAME" get_submodule_config }");
	return config;
}

#define TAMEX_INIT_RD(a0, a1, label) do {\
	if (!gsi_pex_slave_read(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%"PRIz":card=%"PRIz" read("#a0\
		    ", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)
#define TAMEX_INIT_WR(a0, a1, label) do {\
	if (!gsi_pex_slave_write(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%"PRIz":card=%"PRIz" write("#a0\
		    ", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)

int
gsi_tamex_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct ModuleGate main_gate;
	struct GsiTamexModule *tam;
	struct GsiPex *pex;
	size_t card_i, card_num, sfp_i;
	uint32_t fifo_length_bits;
	int do_data_red, do_padi_or_global, do_padi_combine_global,
	    do_long_range, ret;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");
	ret = 0;
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	pex = crate_gsi_pex_get(a_crate);

	do_data_red = config_get_boolean(tam->module.config,
	    KW_DATA_REDUCTION);
	do_padi_or_global = config_get_boolean(tam->module.config,
	    KW_PADI_OR);
	do_padi_combine_global = config_get_boolean(tam->module.config,
	    KW_PADI_COMBINE);
	do_long_range = config_get_boolean(tam->module.config, KW_LONG_RANGE);
	fifo_length_bits = config_get_int32(tam->module.config,
	    KW_FIFO_LENGTH, CONFIG_UNIT_NONE, 0, 0x30) << 16;

	gate_get(&main_gate, NULL, tam->module.config, do_long_range);

	sfp_i = tam->sfp_i;
	card_num = tam->card_num;
	LOGF(verbose)(LOGL, "SFP=%"PRIz", cards=%"PRIz".", sfp_i, card_num);

	if (!gsi_pex_slave_init(pex, sfp_i, card_num)) {
		log_error(LOGL,
		    "SFP=%"PRIz":cards=%"PRIz" slave init failed.", sfp_i,
		    card_num);
		goto tamex_init_fast_done;
	}

	for (card_i = 0; card_i < card_num; ++card_i) {
		uint16_t threshold_array[16];
		struct ModuleGate card_gate;
		struct GsiTamexModuleCard *card;
		float pref, postf;
		uint32_t test_pulse_channel_mask;
		uint32_t test_pulse_delay, test_pulse_freq;
		uint32_t pre, post;
		uint32_t ref_ch0_0;
		uint32_t ref_ch0_1;
		uint32_t trigger_window;
		uint32_t channel_enable;
		unsigned clock_i;
		int do_padi_or, do_padi_combine, clock_source,
		    is_thr_independent;

		LOGF(verbose)(LOGL, "Card=%"PRIz"<%"PRIz"", card_i, card_num);
		card = &tam->card_array[card_i];

		/* TODO: This should be common. */
#define GET_BOOLEAN(attr, name) do {\
		enum Keyword const c_boolean[] = {KW_TRUE, KW_FALSE,\
		    KW_INHERIT};\
		int boolean;\
		attr = attr##_global;\
		boolean = CONFIG_GET_KEYWORD(card->config, name, c_boolean);\
		if (KW_TRUE == boolean) {\
			attr = 1;\
		} else if (KW_FALSE == boolean) {\
			attr = 0;\
		}\
	} while (0)

		/* TODO: Clean up this mess. */
	{
uint32_t buf_i, ofs;

TAMEX_INIT_RD(REG_BUF0, &buf_i, tamex_init_fast_done);
LOGF(verbose)(LOGL, "Buf0=%u.", buf_i);
TAMEX_INIT_RD(REG_BUF1, &buf_i, tamex_init_fast_done);
LOGF(verbose)(LOGL, "Buf1=%u.", buf_i);
		TAMEX_INIT_RD(REG_SUBMEM_NUM,
		    &tam->card_array[card_i].tdc_num, tamex_init_fast_done);
		LOGF(verbose)(LOGL, "TDC num=%u.",
		    tam->card_array[card_i].tdc_num);
TAMEX_INIT_RD(REG_SUBMEM_OFF, &ofs, tamex_init_fast_done);
LOGF(verbose)(LOGL, "TDC addr ofs=%u.", ofs);
	}

		TAMEX_INIT_WR(REG_RESET, 1, tamex_init_fast_done);
		TAMEX_INIT_WR(REG_DATA_LEN, 0x10000000, tamex_init_fast_done);
		TAMEX_INIT_WR(REG_DATA_RED, do_data_red,
		    tamex_init_fast_done);
		TAMEX_INIT_WR(REG_MEM_DIS, 0, tamex_init_fast_done);
		TAMEX_INIT_WR(REG_HEADER, sfp_i, tamex_init_fast_done);

		/*
		 * We have to get these even though we don't use them,
		 * otherwise the config code will complain they're unused.
		 * (For once this is a feature, not a bug!)
		 */
		CONFIG_GET_INT_ARRAY(threshold_array, card->config,
		    KW_THRESHOLD, CONFIG_UNIT_NONE, 0, 0x3ff);
		is_thr_independent = config_get_boolean(card->config,
		    KW_INDEPENDENT);
		if (KW_TAMEX2_PADI == tam->model ||
		    KW_TAMEX_PADI1 == tam->model ||
		    KW_TAMEX4_PADI == tam->model) {
			if (!padi_set_threshold(pex, sfp_i, card_i,
			    threshold_array, is_thr_independent)) {
				goto tamex_init_fast_done;
			}
		}
		{
			enum Keyword const c_kw[] =
			    {KW_BACKPLANE, KW_EXTERNAL, KW_INTERNAL};
			clock_source = CONFIG_GET_KEYWORD(tam->module.config,
			    KW_CLOCK_INPUT, c_kw);
		}
		clock_i = 0;
		if (KW_TAMEX2 == tam->model || KW_TAMEX2_PADI == tam->model) {
			if (KW_EXTERNAL == clock_source) {
				clock_i = CLK_SRC_EXT_TAM2;
			} else if (KW_INTERNAL == clock_source) {
				clock_i = CLK_SRC_ONB_TAM2;
			} else {
				log_die(LOGL, "Tamex2 does not support a "
				    "backplane clock!");
			}
		} else if (KW_TAMEX_PADI1 == tam->model ||
		    KW_TAMEX4_PADI == tam->model) {
			if (KW_EXTERNAL == clock_source) {
				clock_i = CLK_SRC_EXT_TAMP1;
			} else if (KW_INTERNAL == clock_source) {
				clock_i = CLK_SRC_ONB_TAMP1;
			} else {
				clock_i = CLK_SRC_BPL_TAMP1;
			}
		} else if (KW_TAMEX3 == tam->model) {
			if (KW_BACKPLANE == clock_source) {
				clock_i = CLK_SRC_BPL_TAM3;
			} else if (KW_INTERNAL == clock_source) {
				clock_i = CLK_SRC_ONB_TAM3;
			} else {
				log_die(LOGL, "Tamex3 does not support an "
				    "external clock!");
			}
		}
		TAMEX_INIT_WR(REG_TAM_CLK_SEL, clock_i, tamex_init_fast_done);

		card->sync_ch = config_get_int32(card->config, KW_SYNC_CH,
		    CONFIG_UNIT_NONE, 0, 15);
		LOGF(verbose)(LOGL, "Sync-ch=%d.", card->sync_ch);

		gate_get(&card_gate, &main_gate, card->config, do_long_range);
		if (card_gate.width_ns < 1e-6) {
			COPY(card_gate, main_gate);
		}
		pref = -card_gate.time_after_trigger_ns;
		postf = card_gate.time_after_trigger_ns + card_gate.width_ns;
		pre = ceil(pref / 5);
		post = ceil(postf / 5);
		trigger_window = post << 16 | pre;
		LOGF(verbose)(LOGL, "Trigger window = "
		    "(pre=%.fns=0x%04x,post=%.fns=0x%04x,tot=%08x)", pref,
		    pre, postf, post, trigger_window);
		/*
		 * Old fw: MSB = enable window, otherwise write everything!
		 * New fw: all bits for pre/post, writing enables window.
		 */
		TAMEX_INIT_WR(REG_TAM_TRG_WIN,
		    (do_long_range ? 0 : 0x80000000) | trigger_window,
		    tamex_init_fast_done);

		if (config_get_boolean(tam->module.config, KW_REF_CH0)) {
			ref_ch0_0 = 0x20d0;
			ref_ch0_1 = 0x20c0;
		} else {
			ref_ch0_0 = 0x2050;
			ref_ch0_1 = 0x2040;
		}
		GET_BOOLEAN(do_padi_or, KW_PADI_OR);
		GET_BOOLEAN(do_padi_combine, KW_PADI_COMBINE);
		if (KW_TAMEX3 == tam->model) {
			do_padi_or = 0;
			do_padi_combine = 0;
		}
		do_padi_or = do_padi_or ? 1 << 29 : 0;
		do_padi_combine = do_padi_combine ? 1 << 28 : 0;
		TAMEX_INIT_WR(REG_TAM_CTRL, ref_ch0_0 | fifo_length_bits,
		    tamex_init_fast_done);
		TAMEX_INIT_WR(REG_TAM_CTRL, ref_ch0_1 | do_padi_or |
		    do_padi_combine | fifo_length_bits, tamex_init_fast_done);

		/* TODO: bit 0 = ch0 lead, bit 1 = ch 0 trail... */
		channel_enable = config_get_bitmask(card->config,
		    KW_CHANNEL_ENABLE, 0, 31);
		LOGF(verbose)(LOGL, "Channel bitmask=0x%08x.",
		    channel_enable);
		/* TODO: Why can't this be channel_enable? */
		TAMEX_INIT_WR(REG_TAM_EN_CH, 0xffffffff,
		    tamex_init_fast_done);
		/* TODO: Trigger enable config? */
		TAMEX_INIT_WR(REG_TAM_TRG_EN, channel_enable,
		    tamex_init_fast_done);
		/* TODO: Polarity bitmask:
		 *  0 = negative
		 *  1 = positive
		 */
		TAMEX_INIT_WR(REG_TAM_POLARITY, 0,
		    tamex_init_fast_done);

		test_pulse_channel_mask = config_get_bitmask(card->config,
		    KW_TEST_PULSE_CHANNEL, 0, 31);

		test_pulse_delay = config_get_int32(card->config,
		    KW_TEST_PULSE_DELAY, CONFIG_UNIT_NS, 0, 120);
		test_pulse_delay = (test_pulse_delay + 7) & 0xf8;
		LOGF(verbose)(LOGL, "Test pulse delay=%d ns.",
		    test_pulse_delay);
		test_pulse_delay >>= 3;

		test_pulse_freq = config_get_int32(card->config,
		    KW_TEST_PULSE_FREQ, CONFIG_UNIT_KHZ, 30, 120);
		if (30 != test_pulse_freq && 120 != test_pulse_freq) {
			log_die(LOGL, "Test pulse frequency must be "
			    "30 or 120 kHz, is %d kHz.", test_pulse_freq);
		}
		LOGF(verbose)(LOGL, "Test pulse freq=%d kHz.",
		    test_pulse_freq);
		test_pulse_delay /= 120;

		if (KW_TAMEX4_PADI == tam->model) {
			uint32_t test_pulse_on;

			if (0 == test_pulse_channel_mask) {
				test_pulse_delay = 0;
				test_pulse_freq = 0;
				test_pulse_on = 0;
			} else {
				LOGF(info)(LOGL, "TEST PULSE ON.");
				test_pulse_on = 1;
			}
			TAMEX_INIT_WR(REG_TAM_MISC1, test_pulse_channel_mask,
			    tamex_init_fast_done);
			TAMEX_INIT_WR(REG_TAM_MISC2,
					((test_pulse_freq << 5)
					 | (test_pulse_on << 4)
					 | test_pulse_delay),
					tamex_init_fast_done);
		} else if (0 != test_pulse_channel_mask) {
			log_die(LOGL,
			    "Test pulse only supported for TAMEX4_PADI!");
		}
	}

	tam->pex_buf_idx = pex->buf_idx;
	ret = 1;
tamex_init_fast_done:
	LOGF(info)(LOGL, NAME" init_fast }");
	return ret;
}

int
gsi_tamex_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiTamexModule *tam;
	struct GsiPex *pex;
	size_t card_num, sfp_i;

	LOGF(info)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	pex = crate_gsi_pex_get(a_crate);
	sfp_i = tam->sfp_i;
	card_num = tam->card_num;
	LOGF(verbose)(LOGL, "SFP=%"PRIz", cards=%"PRIz".", sfp_i, card_num);
	gsi_pex_sfp_tag(pex, sfp_i);
	LOGF(info)(LOGL, NAME" init_slow }");
	return 1;
}

void
gsi_tamex_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_tamex_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct GsiTamexModule *tam;
	struct GsiPex *pex;
	uint32_t const *p32, *end;
	uint8_t const *p8;
	uint32_t card_i, tdc_i;
	unsigned lec_glob, err_count;
	int ret;
	int sync_l = -1, sync_t = -1;

	(void)a_do_pedestals;
	ret = 0;

	LOGF(spam)(LOGL, NAME" parse {");
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	pex = crate_gsi_pex_get(a_crate);
	lec_glob = 0xffff & a_module->event_counter.value;
	err_count = 0;
	p32 = a_event_buffer->ptr;
	p8 = (void const *)p32;
	end = (void const *)(p8 + a_event_buffer->bytes);
	for (;;) {
		struct GsiTamexModuleCard const *card;
		uint32_t w, bytes;

		if (p32 == end || p32 + 1 == end) {
			break;
		}
		/* Header, count, errors, footer. */
		if (!MEMORY_CHECK(*a_event_buffer, &p32[3])) {
			log_error(LOGL, "Too little TDC data.");
			ret |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_tamex_parse_data_done;
		}
		/* TDC header. */
		w = *p32;
		card_i = (w & 0x00ff0000) >> 16;
		if (0x34 != (w & 0xff)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header (0x%08x,card=%u?) does not have "
			    "0x34.", w, card_i);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_tamex_parse_data_done;
		}
		if (tam->gsi_mbs_trigger != ((w & 0xf00) >> 8)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "card[%u] TDC header (0x%08x) does not have TRIVA"
			    " trigger %u.", card_i, w, tam->gsi_mbs_trigger);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_tamex_parse_data_done;
		}
		if (card_i >= tam->card_num) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "card[%u] TDC header (0x%08x) has card > "
			    "configured=%"PRIz".", card_i, w, tam->card_num);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_tamex_parse_data_done;
		}
		card = &tam->card_array[card_i];
		tdc_i = (w & 0xff000000) >> 24;
		if (tdc_i >= card->tdc_num) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "card[%u] TDC header (0x%08x) has TDC=%u > "
			    "configured=%u.", card_i, w, tdc_i,
			    card->tdc_num);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_tamex_parse_data_done;
		}
		++p32;
		bytes = *p32;
		if (0 == bytes) {
			/* Data reduction. */
			++p32;
			continue;
		}
		/* Check memory up to and including footer. */
		p8 = (void const *)p32;
		if (!MEMORY_CHECK(*a_event_buffer, &p8[bytes])) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "card[%u] TDC data says size=0x%08x, but "
			    "0x%"PRIzx" available.", card_i, bytes,
			    a_event_buffer->bytes);
			ret |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_tamex_parse_data_done;
		}
#define CHECK_MARKERS(type, a_head) do {\
		unsigned head, trigger, buf_idx;\
		uint16_t lec;\
		head = (0xff000000 & w) >> 24;\
		if (a_head != head) {\
			module_parse_error(LOGL, a_event_buffer, p32,\
			    "card[%u] TDC data "type" marker 0x%02x != "\
			    "expected marker "#a_head".", card_i, head);\
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;\
			goto gsi_tamex_parse_data_done;\
		}\
		trigger = (0xf00000 & w) >> 20;\
		if (tam->gsi_mbs_trigger != trigger) {\
			module_parse_error(LOGL, a_event_buffer, p32,\
			    "card[%u] TDC "type" trigger %u != expected "\
			    "trigger %u.", card_i, trigger,\
			    tam->gsi_mbs_trigger);\
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;\
			goto gsi_tamex_parse_data_done;\
		}\
		buf_idx = (0xf0000 & w) >> 16;\
		if (pex->buf_idx != buf_idx) {\
			module_parse_error(LOGL, a_event_buffer, p32,\
			    "card[%u] TDC "type" buf_idx %u != expected "\
			    "buf_idx %u.", card_i, buf_idx, pex->buf_idx);\
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;\
			goto gsi_tamex_parse_data_done;\
		}\
		lec = 0xffff & w;\
		if (lec_glob != lec) {\
			module_parse_error(LOGL, a_event_buffer, p32,\
			    "card[%u] TDC "type" LEC %u != expected LEC %u.",\
			    card_i, lec, lec_glob);\
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;\
			goto gsi_tamex_parse_data_done;\
		};\
	} while (0)
		/* TDC data header. */
		++p32;
		w = *p32;
		CHECK_MARKERS("header", 0xaa);
#if 0
		/* Skip TDC data and error. */
		p32 += bytes / 4 - 1;
#else
		/* Look for 0x3ff fine times. */
		{
			uint32_t i, num;

			/* Skip header. */
			++p32;
			num = bytes / 4 - 3;
			for (i = 0; i < num; ++i, ++p32) {
				uint32_t fine;

				w = *p32;
				if (0x4 != (w >> 29)) {
					continue;
				}
				fine = (0x003ff000 & w) >> 12;
				if (0x3ff == fine) {
					log_error(LOGL, "card[%u] TDC data "
					    "(0x%08x) has fine time=0x3ff!",
					    card_i, w);
					++err_count;
				} else if (card->sync_ch >= 0) {
					int ch;

					ch = (0x1fc00000 & w) >> 22;
		/*
		 * tamex can not measure long signals because of AC coupling.
		 * need to take leading from first input (thr=0x0), then
		 * take trailing edge info from second channel (thr=0x300).
		 */
					if (2 * card->sync_ch + 2 == ch) {
						/* only take first hit */
						if (sync_l == -1) {
							sync_l = 0x000007ff & w;
						}
					} else if (2 * card->sync_ch + 4 ==
					    ch) {
						/* only take first hit */
						if (sync_t == -1) {
							sync_t = 0x000007ff & w;
						}
					}
				}
			}
			/* Skip error bits. */
			++p32;
		}
#endif
		/* TDC footer. */
		w = *p32;
		CHECK_MARKERS("footer", 0xbb);
		++p32;
	}
	if (err_count > 8) {
		log_error(LOGL, "Too many 0x3ff fine times, "
		    "something is wrong!");
		ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto gsi_tamex_parse_data_done;
	}
	if (-1 != sync_l && -1 != sync_t) {
		int tot;

		tot = (((sync_t - sync_l + 2048 + 1024) & 2047) - 1024);
		crate_sync_push(a_crate, tot);
	}
gsi_tamex_parse_data_done:
	LOGF(spam)(LOGL, NAME" parse(0x%08x) }", ret);
	return ret;
}

uint32_t
gsi_tamex_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct EventBuffer eb;
	struct GsiPex *pex;
	struct GsiTamexModule *tam;
	struct GsiTamexCrate *crate;
	uintptr_t dst_bursted, phys_minus_virt, burst_mask;
	uint32_t bytes, bytes_bursted;
	unsigned burst, trial;
	int ret;

	LOGF(spam)(LOGL, NAME" readout {");
	ret = 0;

	pex = crate_gsi_pex_get(a_crate);
	crate = crate_get_tamex_crate(a_crate);
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);

	/* Always do large bursts, we move the data to recover padding. */
	burst = 0x80;
	if (pex->is_parallel) {
		/*
		 * Token request already done in parallel on configured SFP:s
		 * in crate_readout_prepare, now we wait for this particular
		 * SFP to be ready.
		 */
		if (!gsi_pex_token_receive(pex, tam->sfp_i,
		    crate->sfp[tam->sfp_i]->card_num)) {
			log_error(LOGL, NAME":SFP=%"PRIz": Failed to receive"
			    " token.", tam->sfp_i);
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_tamex_readout_done;
		}
		/*
		 * Shouldn't have to add a uint32_t to get the footer, but
		 * such is life.
		 */
		bytes = GSI_PEX_READ(pex, tk_mem_sizen(tam->sfp_i));
		if (0 < bytes) {
			bytes += sizeof(uint32_t);
		}
	} else {
		bytes = 0;
		/* TODO: This could probably be optimized somehow... */
	}

	/* Let's grab the data, folks! */

/* TODO: Check Sicy vs DMA. */
if (1) {

	assert(IS_POW2(burst));
	LOGF(spam)(LOGL, "bytes=0x%08x burst=0x%08x.", bytes, burst);
	/* Adjust buffer to 'burst' boundary. */
	burst_mask = burst - 1;
	COPY(eb, *a_event_buffer);
	gsi_pex_buf_get(pex, &eb, &phys_minus_virt);
	dst_bursted = ((uintptr_t)eb.ptr + burst_mask) & ~burst_mask;
	bytes_bursted = (bytes + burst_mask) & ~burst_mask;
	if (dst_bursted + bytes_bursted > (uintptr_t)eb.ptr + eb.bytes) {
		log_error(LOGL, NAME":SFP=%"PRIz": Wanted to read %u B, "
		    "but only %"PRIz"B event memory available.", tam->sfp_i,
		    bytes, eb.bytes);
		ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto gsi_tamex_readout_done;
	}
	if (pex->is_parallel) {
		/* Setup DMA, and do it! */
		LOGF(spam)(LOGL, "SFP=%"PRIz" DMA src=%p dst=%p->%p "
		    "size=0x%08x->0x%08x burst=0x%08x.",
		    tam->sfp_i,
		    (void *)pex->sfp[tam->sfp_i].phys,
		    eb.ptr,
		    (void *)dst_bursted,
		    bytes,
		    bytes_bursted,
		    burst);
		if (0 < bytes_bursted) {
			pex->dma->dst = phys_minus_virt + dst_bursted;
			pex->dma->transport_size = bytes_bursted;
			pex->dma->burst_size = burst;
			/* The next line will start the DMA with v5 FW. */
			pex->dma->src = pex->sfp[tam->sfp_i].phys;
			if (!pex->is_v5) {
				pex->dma->stat = 1;
			}
		}
	} else {
		LOGF(spam)(LOGL, "SFP=%"PRIz" DMA dst=%p->%p burst=0x%08x.",
		    tam->sfp_i,
		    eb.ptr,
		    (void *)dst_bursted,
		    burst);
		pex->dma->stat = 1 << (1 + tam->sfp_i);
		pex->dma->dst = phys_minus_virt + dst_bursted;
		/*
		 * Now we request data on a single SFP to go all the way to
		 * the DMA target.
		 */
		gsi_pex_token_issue_single(pex, tam->sfp_i);
		if (!gsi_pex_token_receive(pex, tam->sfp_i,
		    crate->sfp[tam->sfp_i]->card_num)) {
			log_error(LOGL, NAME":SFP=%"PRIz":Trig=%u: Failed to "
			    "receive token.", tam->sfp_i,
			    tam->gsi_mbs_trigger);
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_tamex_readout_done;
		}
	}
	/* And now we twiddle our thumbs... */
	for (trial = 0;;) {
		uint32_t stat;

		stat = pex->dma->stat;
		if (0 == (1 & stat)) {
			break;
		}
		if (0xffffffff == stat) {
			log_error(LOGL, "PCIe bus error.");
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_tamex_readout_done;
		}
		++trial;
		if (0 == trial % 1000000) {
			log_error(LOGL, "DMA not finished, trial=%u (PC may "
			    "need cold reboot).", trial);
		}
		sched_yield();
	}
	if (!pex->is_parallel) {
		bytes = pex->dma->transport_size;
		pex->dma->stat = 0;
		/* TODO: In original impl, but seems not needed. */
		/*pex->dma->transport_size = 0;*/
		LOGF(spam)(LOGL, "Sequential bytes = %u.", bytes);
		if (bytes > a_event_buffer->bytes) {
			log_error(LOGL, NAME":SFP=%"PRIz": Sequential "
			    "readout got %u B, but only %"PRIz" B event "
			    "memory available, buffer overflow!", tam->sfp_i,
			    bytes, a_event_buffer->bytes);
			ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto gsi_tamex_readout_done;
		}
	}
	/* Move back data to cover DMA address alignment fudging. */
	memmove(a_event_buffer->ptr, (void *)dst_bursted, bytes);
 } else {
	/* Let's do it the stupid way. */
	uint32_t volatile *src;
	uint32_t *dst;
	uint32_t i;

	dst = a_event_buffer->ptr;
	src = pex->sfp[tam->sfp_i].virt;
	for (i = 0; i < bytes; i += 4) {
		*dst++ = *src++;
	}
 }
	EVENT_BUFFER_ADVANCE(*a_event_buffer, (uint8_t *)a_event_buffer->ptr +
	    bytes);

gsi_tamex_readout_done:
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", ret);
	return ret;
}

uint32_t
gsi_tamex_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiTamexModule *tam;
	struct GsiPex *pex;

	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	pex = crate_gsi_pex_get(a_crate);
	if (pex->buf_idx != tam->pex_buf_idx) {
		++a_module->event_counter.value;
		tam->pex_buf_idx = pex->buf_idx;
	}
	tam->gsi_mbs_trigger = crate_gsi_mbs_trigger_get(a_crate);
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x) }",
	    a_module->event_counter.value);
	return 0;
}

void
gsi_tamex_setup_(void)
{
	MODULE_SETUP(gsi_tamex, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(gsi_tamex, get_submodule_config);
	MODULE_CALLBACK_BIND(gsi_tamex, sub_module_pack);
}

void
gsi_tamex_sub_module_pack(struct Module *a_module, struct PackerList *a_list)
{
	struct GsiTamexModule *tam;
	struct Packer *packer;
	size_t i;

	LOGF(debug)(LOGL, NAME" sub_module_pack {");
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	packer = packer_list_get(a_list, 8);
	pack8(packer, tam->card_num);
	for (i = 0; i < tam->card_num; ++i) {
		packer = packer_list_get(a_list, 16);
		pack16(packer, KW_GSI_TAMEX_CARD);
	}
	LOGF(debug)(LOGL, NAME" sub_module_pack }");
}

int
padi_set_threshold(struct GsiPex *a_pex, unsigned a_sfp_i, unsigned a_card_i,
    uint16_t const *a_threshold_array, int a_is_thr_independent)
{
	/* Name conversions so we can use TAMEX_INIT_WR. */
	struct GsiPex *pex = a_pex;
	size_t sfp_i = a_sfp_i;
	size_t card_i = a_card_i;
	unsigned ch_i;

	if (a_is_thr_independent) {
		for (ch_i = 0; ch_i < 8; ++ch_i) {
			unsigned ofs0, ofs8;

			ofs0 = ch_i;
			ofs8 = ofs0 + 8;
			/* Write to 2 ch (i & i+8) at once. */
			TAMEX_INIT_WR(REG_TAM_PADI_DAT,
			    0x80008000 | /* Enable both PADI:s. */
			    /* Channel i. */
			    (ch_i << 10) |
			    (a_threshold_array[ofs0]) |
			    /* Channel i+8. */
			    (ch_i << 26) |
			    (a_threshold_array[ofs8] << 16),
			    padi_set_threshold_done);
			time_sleep(500e-6);
			/* Magic. */
			TAMEX_INIT_WR(REG_TAM_PADI_CTL, 1,
			    padi_set_threshold_done);
			/* More magic. */
			time_sleep(500e-6);
			/* Cannot have enough magic. */
			TAMEX_INIT_WR(REG_TAM_PADI_CTL, 0,
			    padi_set_threshold_done);
			time_sleep(500e-6);
		}
	} else {
		uint32_t value;

		value = 0xa000 | a_threshold_array[0];
		value |= value << 16;
		TAMEX_INIT_WR(REG_TAM_PADI_DAT, value,
		    padi_set_threshold_done);
		time_sleep(500e-6);
		TAMEX_INIT_WR(REG_TAM_PADI_CTL, 1,
		    padi_set_threshold_done);
		time_sleep(500e-6);
		TAMEX_INIT_WR(REG_TAM_PADI_CTL, 0,
		    padi_set_threshold_done);
		time_sleep(500e-6);
	}
	return 1;
padi_set_threshold_done:
	return 0;
}

#else

struct Module *
gsi_tamex_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	(void)a_crate;
	(void)a_block;
	log_die(LOGL, NAME" not supported in this build/platform.");
}

void
gsi_tamex_setup_(void)
{
}

#endif

void
gsi_tamex_crate_add(struct GsiTamexCrate *a_crate, struct Module *a_module)
{
	struct GsiTamexModule *tam;
	size_t sfp_i;

	LOGF(verbose)(LOGL, NAME" crate_add {");
	MODULE_CAST(KW_GSI_TAMEX, tam, a_module);
	sfp_i = tam->sfp_i;
	if (NULL != a_crate->sfp[sfp_i]) {
		log_die(LOGL, "Multiple TAMEX chains on SFP=%"PRIz"!",
		    sfp_i);
	}
	a_crate->sfp[sfp_i] = tam;
	LOGF(verbose)(LOGL, NAME" crate_add }");
}

void
gsi_tamex_crate_create(struct GsiTamexCrate *a_crate)
{
	ZERO(*a_crate);
}

int
gsi_tamex_crate_init_slow(struct GsiTamexCrate *a_crate)
{
	size_t i;

	for (i = 0; i < LENGTH(a_crate->sfp); ++i) {
		struct GsiTamexModule *tam;

		tam = a_crate->sfp[i];
		if (NULL != tam) {
			tam->module.event_counter.value = 0;
		}
	}
	return 1;
}
