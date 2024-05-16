/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021-2024
 * Manuel Xarepe
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

#include <module/gsi_ctdc_proto/internal.h>
#include <sched.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_pex/offsets.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/time.h>

#define NAME "Gsi CTDC-proto"

#if NCONF_mGSI_PEX_bYES
#	include <math.h>
#	include <module/map/map.h>
#	include <nurdlib/config.h>
#	include <nurdlib/crate.h>
#	include <nurdlib/log.h>

enum {
	REG_CTDC_CTRL              = 0x200000,
	REG_CTDC_TRIG_DELAY        = 0x200004,
	REG_CTDC_WINDOW_LEN        = 0x200008,
	REG_CTDC_TIME_RESET        = 0x200020
};
static const uint32_t REG_CTDC_CHANNEL_DISABLE[] = {
	0x200010,
	0x200014,
	0x200018,
	0x20001c,
	0x2000a0
};
static const uint32_t REG_CTDC_TRIGGER_DISABLE[] = {
	0x200030,
	0x200034,
	0x200038,
	0x20003c,
	0x2000a4
};

#define CTDCP_INIT_RD(a0, a1, label) do {\
	if (!gsi_pex_slave_read(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%u:card=%u "\
		    "read("#a0", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)
#define CTDCP_INIT_WR(a0, a1, label) do {\
	if (!gsi_pex_slave_write(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%u:card=%u "\
		    "write("#a0", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)

void
gsi_ctdc_proto_create(struct ConfigBlock *a_block, struct GsiCTDCProtoModule
    *a_ctdcp, enum Keyword a_child_type)
{
	struct ConfigBlock *card_block;
	size_t card_i;

	a_ctdcp->module.event_max = 1;
	a_ctdcp->module.event_counter.mask = 0;
	a_ctdcp->sfp_i = config_get_block_param_int32(a_block, 0);
	if (a_ctdcp->sfp_i >= 4) {
		log_die(LOGL, NAME" create: SFP=%"PRIz"u != [0,3] does not "
		    "seem right.", a_ctdcp->sfp_i);
	}

	/* Count cards. */
	for (card_block = config_get_block(a_block, a_child_type);
	    NULL != card_block;
	    card_block = config_get_block_next(card_block, a_child_type)) {
		card_i = config_get_block_param_int32(card_block, 0);
		a_ctdcp->card_num = MAX(a_ctdcp->card_num, card_i + 1);
	}
	if (!IN_RANGE_II(a_ctdcp->card_num, 1, 256)) {
		log_die(LOGL, NAME" create: Card-num=%"PRIz" != [1,256], "
		    "please add some CTDC cards in your config.",
		    a_ctdcp->card_num);
	}
	CALLOC(a_ctdcp->card_array, a_ctdcp->card_num);
	for (card_block = config_get_block(a_block, a_child_type);
	    NULL != card_block;
	    card_block = config_get_block_next(card_block, a_child_type)) {
		struct GsiCTDCProtoCard *card;

		card_i = config_get_block_param_int32(card_block, 0);
		card = &a_ctdcp->card_array[card_i];
		card->config = card_block;
		card->frequency = config_get_block_param_int32(card_block, 1);
		if (150 != card->frequency && 250 != card->frequency) {
			log_die(LOGL, NAME" create: Invalid clock frequency "
			    "%u, only  150 and 250 accepted.",
			    card->frequency);
		}
	}
	a_ctdcp->config = a_block;
}

void
gsi_ctdc_proto_destroy(struct GsiCTDCProtoModule *a_ctdcp)
{
	LOGF(verbose)(LOGL, NAME" destroy {");
	FREE(a_ctdcp->card_array);
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct ConfigBlock *
gsi_ctdc_proto_get_submodule_config(struct GsiCTDCProtoModule *a_ctdcp,
    unsigned a_i)
{
	struct ConfigBlock *config;

	LOGF(verbose)(LOGL, NAME" get_submodule_config(%u) {", a_i);
	config = a_i < a_ctdcp->card_num ? a_ctdcp->card_array[a_i].config :
	    NULL;
	LOGF(verbose)(LOGL, NAME" get_submodule_config }");
	return config;
}

int
gsi_ctdc_proto_init_fast(struct Crate *a_crate, struct GsiCTDCProtoModule
    *a_ctdcp, unsigned const *a_clock_switch)
{
	enum Keyword const c_kw[] = {KW_EXTERNAL, KW_INTERNAL};
	struct GsiPex *pex;
	enum Keyword clock_source;
	unsigned card_i, card_num, sfp_i;
	unsigned data_format;
	int clock_i, ret;
	uint16_t threshold_offset;

	LOGF(verbose)(LOGL, NAME" init_fast {");

	pex = crate_gsi_pex_get(a_crate);
	sfp_i = a_ctdcp->sfp_i;
	card_num = a_ctdcp->card_num;
	LOGF(verbose)(LOGL, "SFP=%u,cards=%u.", sfp_i, card_num);
	ret = 0;

	if (!gsi_pex_slave_init(pex, sfp_i, card_num)) {
		log_error(LOGL, "SFP=%u,cards=%u slave init failed.", sfp_i,
		    card_num);
		goto ctdc_init_fast_done;
	}

	data_format = config_get_int32(a_ctdcp->module.config, KW_DATA_FORMAT,
	    CONFIG_UNIT_NONE, 0, 8);

	threshold_offset = config_get_int32(a_ctdcp->module.config,
	    KW_THRESHOLD_OFFSET, CONFIG_UNIT_NONE, -0xffff, 0xffff);

	clock_source = CONFIG_GET_KEYWORD(a_ctdcp->module.config,
	    KW_CLOCK_INPUT, c_kw);
	clock_i = a_clock_switch[KW_INTERNAL == clock_source ? 0 : 1];

	switch (data_format) {
	case 1:
	case 3:
	case 7:
		break;
	default:
		log_error(LOGL, "'data_format' has the wrong value %u, do "
		    "you really know what you're doing?", data_format);
		data_format = 1;
	}

	for (card_i = 0; card_i < card_num; ++card_i) {
		uint16_t threshold_array[128];
		uint32_t channel_disable[LENGTH(REG_CTDC_CHANNEL_DISABLE)];
		uint32_t trigger_disable[LENGTH(REG_CTDC_TRIGGER_DISABLE)];
		struct ModuleGate gate;
		struct GsiCTDCProtoCard const *card;
		struct ConfigBlock *card_block;
		float period;
		unsigned part_i, part_n, channels_to_read;
		uint32_t reg_ctdc_ctrl, delay, width, threshold_offset_local;
		unsigned trigger_delay, window_length;

		LOGF(verbose)(LOGL, "Card=%u<%u", card_i, card_num);
		card = &a_ctdcp->card_array[card_i];
		card_block = card->config;

		/* Select clock source. */
		{
#if 0
			uint32_t clock_status;
#endif

			CTDCP_INIT_WR(REG_CTDC_TIME_RESET, clock_i,
			    ctdc_init_fast_done);
#if 0
			/* This cannot be done for int/ext. */
			CTDCP_INIT_RD(REG_CTDC_TIME_RESET, &clock_status,
			    ctdc_init_fast_done);
			if (0x0 == (0x10 & clock_status)) {
				log_error(LOGL, NAME" SFP=%u:card=%u: Could "
				    "not set clock source.", sfp_i, card_i);
				goto ctdc_init_fast_done;
			}
#endif
		}

		/* Disable test data length. */
		CTDCP_INIT_WR(REG_DATA_LEN, 0x10000000, ctdc_init_fast_done);

		/* Disable trigger accept. */
		CTDCP_INIT_WR(REG_CTDC_CTRL, 0, ctdc_init_fast_done);
		CTDCP_INIT_RD(REG_CTDC_CTRL, &reg_ctdc_ctrl,
		    ctdc_init_fast_done);
		if (0 != (1 & reg_ctdc_ctrl)) {
			log_error(LOGL, NAME" SFP=%u:card=%u: Could not set "
			    "trigger acceptance.", sfp_i, card_i);
			goto ctdc_init_fast_done;
		}

		/* Enable trigger accept by setting data format. */
		CTDCP_INIT_WR(REG_CTDC_CTRL, data_format,
		    ctdc_init_fast_done);
		CTDCP_INIT_RD(REG_CTDC_CTRL, &reg_ctdc_ctrl,
		    ctdc_init_fast_done);
		if (1 != (1 & reg_ctdc_ctrl)) {
			log_error(LOGL, NAME" SFP=%u:card=%u: Could not set "
			    "trigger acceptance.", sfp_i, card_i);
			goto ctdc_init_fast_done;
		}
		/* Calc gate per card, in the ugly case of mixed FW. */
		period = 1000. / card->frequency;
		module_gate_get(&gate, a_ctdcp->module.config,
		    -0xfff * period, 0, 0, 0xfff * period);
		window_length = ceil(gate.width_ns / period);
		/* Delay = time from trigger to end of window. */
		trigger_delay = window_length -
		    ceil(-gate.time_after_trigger_ns / period);
		LOGF(verbose)(LOGL, "Trigger window = "
		    "(start=%.fns,len=%.fns => delay=0x%04x,len=0x%04x)",
		    gate.time_after_trigger_ns, gate.width_ns, trigger_delay,
		    window_length);

		CTDCP_INIT_WR(REG_CTDC_TRIG_DELAY, trigger_delay,
		    ctdc_init_fast_done);
		CTDCP_INIT_WR(REG_CTDC_WINDOW_LEN, window_length,
		    ctdc_init_fast_done);

		CTDCP_INIT_RD(REG_CTDC_TRIG_DELAY, &delay,
		    ctdc_init_fast_done);
		CTDCP_INIT_RD(REG_CTDC_WINDOW_LEN, &width,
		    ctdc_init_fast_done);
		LOGF(verbose)(LOGL,
		    "Window readback = (delay=%u,width=%u) = [%d..%d]ns",
		    delay, width, (int)(period * (delay - width)),
		    (int)(period * delay));

		/* Enable channels + triggers. */
#define GET_DISABLE(name, NAME, part, last_bit) \
	name##_disable[part] = ~config_get_bitmask(card_block,\
	    KW_##NAME##part##_ENABLE, 0, last_bit)
		GET_DISABLE(channel, CHANNEL, 0, 31);
		GET_DISABLE(channel, CHANNEL, 1, 31);
		GET_DISABLE(channel, CHANNEL, 2, 31);
		GET_DISABLE(channel, CHANNEL, 3, 31);
		GET_DISABLE(channel, CHANNEL, 4,  0);
		GET_DISABLE(trigger, TRIGGER, 0, 31);
		GET_DISABLE(trigger, TRIGGER, 1, 31);
		GET_DISABLE(trigger, TRIGGER, 2, 31);
		GET_DISABLE(trigger, TRIGGER, 3, 31);
		GET_DISABLE(trigger, TRIGGER, 4,  0);
		channels_to_read = config_get_int32(card_block,
		    KW_CHANNELS_TO_READ, CONFIG_UNIT_NONE, 128, 129);
		part_n = 128 == channels_to_read ? 4 : 5;
		for (part_i = 0; part_i < part_n; ++part_i) {
			CTDCP_INIT_WR(REG_CTDC_CHANNEL_DISABLE[part_i],
			    channel_disable[part_i], ctdc_init_fast_done);
		}
		for (part_i = 0; part_i < part_n; ++part_i) {
			CTDCP_INIT_WR(REG_CTDC_TRIGGER_DISABLE[part_i],
			    trigger_disable[part_i], ctdc_init_fast_done);
		}

		CTDCP_INIT_WR(REG_HEADER, sfp_i, ctdc_init_fast_done);
		CTDCP_INIT_WR(REG_RESET, 1, ctdc_init_fast_done);

		threshold_offset_local = config_get_int32(card_block,
		    KW_THRESHOLD_OFFSET, CONFIG_UNIT_NONE, -0xffff,
		    0xfffffff);
		if (0xfffffff == threshold_offset_local) {
			threshold_offset_local = threshold_offset;
		}
		CONFIG_GET_INT_ARRAY(threshold_array, card_block,
		    KW_THRESHOLD, CONFIG_UNIT_NONE, -0xffff, 0xffff);
		if (!a_ctdcp->threshold_set(card_block, pex, sfp_i, card_i,
		    threshold_offset_local, threshold_array)) {
			goto ctdc_init_fast_done;
		}
	}
	a_ctdcp->module.event_counter.value = 0;

	ret = 1;
ctdc_init_fast_done:
	LOGF(verbose)(LOGL, NAME" init_fast }");
	return ret;
}

void
gsi_ctdc_proto_init_slow(struct Crate *a_crate, struct GsiCTDCProtoModule
    *a_ctdcp)
{
	struct GsiPex *pex;
	unsigned card_num, sfp_i;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_slow {");
	pex = crate_gsi_pex_get(a_crate);
	sfp_i = a_ctdcp->sfp_i;
	card_num = a_ctdcp->card_num;
	LOGF(verbose)(LOGL, "SFP=%u:cards=%u.", sfp_i, card_num);
	gsi_pex_sfp_tag(pex, a_ctdcp->sfp_i);
	LOGF(verbose)(LOGL, NAME" init_slow }");
}

uint32_t
gsi_ctdc_proto_parse_data(struct Crate const *a_crate, struct
    GsiCTDCProtoModule *a_ctdcp, struct EventConstBuffer const
    *a_event_buffer)
{
	uint32_t const *p32, *end;
	unsigned gsi_mbs_trigger, sfp_i, lec_glob;
	int ret;

	ret = 0;
	LOGF(spam)(LOGL, NAME" parse {");
	gsi_mbs_trigger = crate_gsi_mbs_trigger_get(a_crate);

	lec_glob = 0xffff & a_ctdcp->module.event_counter.value;
	sfp_i = a_ctdcp->sfp_i;
	p32 = a_event_buffer->ptr;
	if (0 != (3 & a_event_buffer->bytes)) {
		log_error(LOGL, "Invalid event buffer alignment.");
		return CRATE_READOUT_FAIL_ERROR_DRIVER;
	}
	end = p32 + a_event_buffer->bytes / 4;
	for (;;) {
		uint32_t u32, data_header, data_num;
		unsigned card_i, ch_i, trig;

		if (p32 > end) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Parsed beyond limit, confused.");
			ret |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_ctdc_proto_parse_data_done;
		}
		if (p32 == end) {
			break;
		}
		/* Must have at least 8 32-bitters. */
		if (!MEMORY_CHECK(*a_event_buffer, &p32[7])) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Missing data.");
			ret |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_ctdc_proto_parse_data_done;
		}
		/* TDC header. */
		u32 = p32[0];
		if (0x34 != (u32 & 0x000000ff)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header does not have 0x34.");
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_ctdc_proto_parse_data_done;
		}
		trig = (u32 & 0x00000f00) >> 8;
		if (gsi_mbs_trigger != trig) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header (0x%08x) has trigger %u, should be "
			    "%u.", u32, trig, gsi_mbs_trigger);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_ctdc_proto_parse_data_done;
		}
		if (sfp_i != ((u32 & 0x0000f000) >> 12)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header does not have SFP ID %u.", sfp_i);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_ctdc_proto_parse_data_done;
		}
		card_i = (u32 & 0x00ff0000) >> 16;
		if (a_ctdcp->card_num <= card_i) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header has invalid card ID %u >= num "
			    "%"PRIz".", card_i, a_ctdcp->card_num);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_ctdc_proto_parse_data_done;
		}
		ch_i = (u32 & 0xff000000) >> 24;
		if (ch_i >= 16 && ch_i != 0xff) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC header has invalid channel %u.", ch_i);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_ctdc_proto_parse_data_done;
		}
		if (0xff != ch_i) {
			uint32_t trace_head, lec;

			trace_head = p32[2];
			lec = 0xffff & trace_head;
			if (lec_glob != lec) {
				log_error(LOGL, "Trace header LEC %u != "
				    "expected LEC %u.", lec, lec_glob);
				ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
				goto gsi_ctdc_proto_parse_data_done;
			}
		}
		/* Data header.*/
		data_header = p32[6];
		if ((0x00ffffff & u32) != (0x00ffffff & data_header)) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "TDC 0x%08x and data 0x%08x headers not equal.",
			    u32, data_header);
			ret |= CRATE_READOUT_FAIL_DATA_CORRUPT;
			goto gsi_ctdc_proto_parse_data_done;
		}
		data_num = p32[7] / sizeof(uint32_t);
		if (!MEMORY_CHECK(*a_event_buffer, &p32[7 + data_num])) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Less TDC data than reported.");
			ret |= CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_ctdc_proto_parse_data_done;
		}
		p32 += 8 + data_num;
	}

gsi_ctdc_proto_parse_data_done:
	LOGF(spam)(LOGL, NAME" parse }");
	return ret;
}

uint32_t
gsi_ctdc_proto_readout(struct Crate *a_crate, struct GsiCTDCProtoModule
    *a_ctdcp, struct EventBuffer *a_event_buffer)
{
	struct EventBuffer eb;
	struct GsiPex *pex;
	struct GsiCTDCCrate *crate;
	uintptr_t dst_bursted, phys_minus_virt, burst_mask;
	uint32_t bytes, bytes_bursted;
	unsigned burst, trial;
	int ret;

	/* Very similar to TAMEX readout, there are even comments there. */
	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");
	ret = 0;

	pex = crate_gsi_pex_get(a_crate);
	crate = crate_get_ctdc_crate(a_crate);

	if (pex->is_parallel) {
/* TODO: Why can we not test the slave #? */
		(void)crate;
		if (!gsi_pex_token_receive(pex, a_ctdcp->sfp_i,
		    /*crate->sfp[a_ctdcp->sfp_i]->card_num*/0)) {
			log_error(LOGL, NAME":SFP=%"PRIz": Failed to receive "
			    "token.", a_ctdcp->sfp_i);
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_ctdc_proto_readout_done;
		}
	}

	if (pex->is_parallel) {
		bytes = GSI_PEX_READ(pex, tk_mem_sizen(a_ctdcp->sfp_i)) +
		    sizeof(uint32_t);
		if (0xa0 > bytes) {
			burst = 0x10;
		} else if (0x140 > bytes) {
			burst = 0x20;
		} else if (0x280 > bytes) {
			burst = 0x40;
		} else {
			burst = 0x80;
		}
	} else {
		bytes = 0;
		burst = 0x80;
	}
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
		    "but only %"PRIz"B event memory available.", a_ctdcp->sfp_i,
		    bytes_bursted, a_event_buffer->bytes);
		ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto gsi_ctdc_proto_readout_done;
	}
	if (pex->is_parallel) {
		LOGF(spam)(LOGL, "DMA src=%p dst=%p->%p size=0x%08x->0x%08x "
		    "burst=0x%08x.",
		    (void *)pex->sfp[a_ctdcp->sfp_i].phys,
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
			pex->dma->src = pex->sfp[a_ctdcp->sfp_i].phys;
			if (!pex->is_v5) {
				pex->dma->stat = 1;
			}
		}
	} else {
		LOGF(spam)(LOGL, "SFP=%"PRIz" DMA dst=%p->%p burst=0x%08x.",
		    a_ctdcp->sfp_i,
		    eb.ptr,
		    (void *)dst_bursted,
		    burst);
		pex->dma->stat = 1 << (1 + a_ctdcp->sfp_i);
		pex->dma->dst = phys_minus_virt + dst_bursted;
		gsi_pex_token_issue_single(pex, a_ctdcp->sfp_i);
		if (!gsi_pex_token_receive(pex, a_ctdcp->sfp_i,
		    /*crate->sfp[a_ctdcp->sfp_i]->card_num*/0)) {
			log_error(LOGL, NAME":SFP=%"PRIz": Failed to "
			    "receive token.", a_ctdcp->sfp_i);
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_ctdc_proto_readout_done;
		}
	}
	for (trial = 0;;) {
		uint32_t stat;

		stat = pex->dma->stat;
		if (0 == (1 & stat)) {
			break;
		}
		if (0xffffffff == stat) {
			log_error(LOGL, "PCIe bus error.");
			ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto gsi_ctdc_proto_readout_done;
		}
		++trial;
		if (0 == trial % 1000000) {
			log_error(LOGL, "DMA not finished, trial=%d (PC may "
			    "need power to be physically pulled).", trial);
		}
		sched_yield();
	}
	if (!pex->is_parallel) {
		bytes = pex->dma->transport_size;
		LOGF(spam)(LOGL, "Sequential bytes = %u.", bytes);
		if (bytes > a_event_buffer->bytes) {
			log_error(LOGL, NAME":SFP=%"PRIz": Sequential "
			    "readout got %u B, but only %"PRIz"B event "
			    "memory available.", a_ctdcp->sfp_i, bytes,
			    a_event_buffer->bytes);
			ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto gsi_ctdc_proto_readout_done;
		}
	}
	memmove(a_event_buffer->ptr, (void *)dst_bursted, bytes);
	EVENT_BUFFER_ADVANCE(*a_event_buffer, (uint8_t *)a_event_buffer->ptr +
	    bytes);

	++a_ctdcp->module.event_counter.value;

gsi_ctdc_proto_readout_done:
	LOGF(spam)(LOGL, NAME" readout }");
	return ret;
}

void
gsi_ctdc_proto_sub_module_pack(struct GsiCTDCProtoModule *a_ctdcp, struct
    PackerList *a_list)
{
	struct Packer *packer;
	size_t i;

	LOGF(debug)(LOGL, NAME" sub_module_pack {");
	packer = packer_list_get(a_list, 8);
	pack8(packer, a_ctdcp->card_num);
	for (i = 0; i < a_ctdcp->card_num; ++i) {
		packer = packer_list_get(a_list, 16);
		pack16(packer, KW_GSI_CTDC_CARD);
	}
	LOGF(debug)(LOGL, NAME" sub_module_pack }");
}

#endif
