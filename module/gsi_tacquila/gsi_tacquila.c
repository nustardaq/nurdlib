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

#include <module/gsi_tacquila/internal.h>
#include <errno.h>
#include <stdarg.h>
#include <module/gsi_sam/internal.h>
#include <module/gsi_sam/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/endian.h>
#include <util/fmtmod.h>
#include <util/string.h>
#include <util/syscall.h>

#define NAME "Gsi Tacquila"

union TacquilaHeader {
	struct {
#if defined(NURDLIB_BIG_ENDIAN)
		uint32_t sam      : 4;
		uint32_t gtb      : 4;
		uint32_t lec      : 4;
		uint32_t trg_sam  : 4;
		uint32_t trg_tacq : 4;
		uint32_t dummy    : 3;
		uint32_t count    : 9;
#elif defined(NURDLIB_LITTLE_ENDIAN)
		uint32_t count    : 9;
		uint32_t dummy    : 3;
		uint32_t trg_tacq : 4;
		uint32_t trg_sam  : 4;
		uint32_t lec      : 4;
		uint32_t gtb      : 4;
		uint32_t sam      : 4;
#endif
	} bits;
  uint32_t u32;
};
union TacquilaData {
	struct {
#if defined(NURDLIB_BIG_ENDIAN)
		uint32_t module   :  5;
		uint32_t channel  :  5;
		uint32_t first    :  1;
		uint32_t trigger  :  1;
		uint32_t clock    :  8; /* Not for ADC values. */
		uint32_t value    : 12;
#elif defined(NURDLIB_LITTLE_ENDIAN)
		uint32_t value    : 12;
		uint32_t clock    :  8; /* Not for ADC values. */
		uint32_t trigger  :  1;
		uint32_t first    :  1;
		uint32_t channel  :  5;
		uint32_t module   :  5;
#endif
	} bits;
	uint32_t u32;
};

static void	free_(struct GsiSamGtbClient **);
static uint32_t	parse_data(struct Crate const *, struct GsiSamGtbClient *,
    struct EventConstBuffer *) FUNC_RETURNS;

#if 0 /* This is not currently used */
static int const c_triva_to_tacq_trg[16] = {
/*	 0  1  2  3  4   5   6   7   8   9  10  11  12  13  14  15 */
	-1,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
#endif

void
free_(struct GsiSamGtbClient **a_client)
{
	FREE(*a_client);
}

void
gsi_tacquila_crate_configure(struct GsiTacquilaCrate *a_crate, struct
    ConfigBlock *a_block)
{
	LOGF(verbose)(LOGL, NAME" crate_configure {");
	FREE(a_crate->data_path);
	FREE(a_crate->util_path);
	a_crate->data_path = strdup_(config_get_string(a_block,
	    KW_DATA_PATH));
	a_crate->util_path = strdup_(config_get_string(a_block,
	    KW_UTIL_PATH));
	LOGF(verbose)(LOGL, "data_path = '%s'.", a_crate->data_path);
	LOGF(verbose)(LOGL, "util_path = '%s'.", a_crate->util_path);
	LOGF(verbose)(LOGL, NAME" crate_configure }");
}

void
gsi_tacquila_crate_add(struct GsiTacquilaCrate *a_crate, struct
    GsiSamGtbClient *a_client)
{
	struct GsiTacquila *tacquila;

	LOGF(verbose)(LOGL, NAME" crate_add {");
	tacquila = (void *)a_client;
	TAILQ_INSERT_TAIL(&a_crate->list, tacquila, next);
	tacquila->crate = a_crate;
	LOGF(verbose)(LOGL, NAME" crate_add }");
}

void
gsi_tacquila_crate_create(struct GsiTacquilaCrate *a_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_create {");
	a_crate->data_path = NULL;
	a_crate->util_path = NULL;
	TAILQ_INIT(&a_crate->list);
	LOGF(verbose)(LOGL, NAME" crate_create }");
}

void
gsi_tacquila_crate_destroy(struct GsiTacquilaCrate *a_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_destroy {");
	FREE(a_crate->data_path);
	FREE(a_crate->util_path);
	LOGF(verbose)(LOGL, NAME" crate_destroy }");
}

int
gsi_tacquila_crate_init_slow(struct GsiTacquilaCrate *a_crate)
{
	struct GsiTacquila *tacquila;
	int ret, sam_mask;

	if (TAILQ_EMPTY(&a_crate->list)) {
		return 1;
	}

	LOGF(verbose)(LOGL, NAME" crate_init_slow {");
	ret = 1;

	TAILQ_FOREACH(tacquila, &a_crate->list, next) {
		size_t ofs;

		ofs = GSI_SAM_GTB_OFFSET(tacquila->sam,
		    tacquila->gtb_client.gtb_i);
		MAP_WRITE_OFS(tacquila->sam->crate->sicy_map, init, ofs, 0);
	}

	LOGF(info)(LOGL, "Loading %s/tacset.txt...", a_crate->data_path);
	system_call("%s/paraload %s/tacset.txt", a_crate->util_path,
	    a_crate->data_path);

	sam_mask = 0;
	TAILQ_FOREACH(tacquila, &a_crate->list, next) {
		int sam_i, bit;

		sam_i = tacquila->sam->address >> 24;
		bit = 1 << sam_i;
		if (0 != (bit & sam_mask)) {
			continue;
		}
		sam_mask |= bit;

		LOGF(info)(LOGL, "Initializing SAM=%d...", sam_i);
		system_call("%s/hstart %d", a_crate->util_path, sam_i);
		system_call(
		    "%s/tacload %d "
		    "%s/tacquila_big_2013.hex "
		    "%s/tacquila_middle_2013.hex", a_crate->util_path, sam_i,
		    a_crate->util_path, a_crate->util_path);
		system_call(
		    "%s/hpistart2 %d "
		    "%s/tacsam5.m0", a_crate->util_path, sam_i,
		    a_crate->util_path);
	}

	TAILQ_FOREACH(tacquila, &a_crate->list, next) {
		size_t ofs;

		int sam_i, gtb_i;

		ofs = GSI_SAM_GTB_OFFSET(tacquila->sam,
		    tacquila->gtb_client.gtb_i);
		sam_i = tacquila->sam->address >> 24;
		gtb_i = tacquila->gtb_client.gtb_i;
		for (;;) {
			uint32_t tacq_init;

			tacq_init =
			    MAP_READ_OFS(tacquila->sam->crate->sicy_map,
				init, ofs);
			tacq_init &= 0xff;
			if (0x88 == tacq_init) {
				log_error(LOGL, "SAM5 %d GTB %d: Tacquila "
				    "FPGA load failed.", sam_i, gtb_i);
				ret = 0;
			}
			if (0x99 == tacq_init) {
				log_error(LOGL, "SAM5 %d GTB %d: GTB "
				    "address initialization failed.", sam_i,
				    gtb_i);
				ret = 0;
			}
			if (0 != tacq_init) {
				LOGF(info)(LOGL, " SAM5 %d GTB %d: %d "
				    "modules found.", sam_i, gtb_i,
				    tacq_init);
				if (tacquila->card_num != tacq_init) {
					log_error(LOGL, "SAM5 %d GTB %d: "
					    "# cards %u != expected %u.",
					    sam_i, gtb_i, tacq_init,
					    tacquila->card_num);
					ret = 0;
				}
				break;
			}
		}
		tacquila->lec = 0;
	}
	LOGF(verbose)(LOGL, NAME" crate_init_slow }");
	return ret;
}

struct GsiSamGtbClient *
gsi_tacquila_create(struct GsiSamModule *a_sam, struct ConfigBlock *a_block)
{
	struct GsiTacquila *tacquila;
	int gtb_i;

	LOGF(verbose)(LOGL, NAME" create {");
	CALLOC(tacquila, 1);
	gtb_i = config_get_block_param_int32(a_block, 0);
	if (!IN_RANGE_II(gtb_i, 0, 1)) {
		log_die(LOGL, NAME" create: GTB=%u does not seem right.",
		    gtb_i);
	}
	GSI_SAM_GTB_CLIENT_CREATE(tacquila->gtb_client, KW_GSI_TACQUILA,
	    gtb_i, free_, parse_data, NULL);
	tacquila->sam = a_sam;
	tacquila->card_num = config_get_block_param_int32(a_block, 1);
	if (!IN_RANGE_II(tacquila->card_num, 1, 256)) {
		log_die(LOGL, NAME" create: Card-num=%u does not seem "
		    "right.", tacquila->card_num);
	}
	LOGF(verbose)(LOGL, NAME" create }");
	return (void *)tacquila;
}

uint32_t
parse_data(struct Crate const *a_crate, struct GsiSamGtbClient *a_client,
    struct EventConstBuffer *a_event_buffer)
{
	struct GsiTacquila *tacquila;
	union TacquilaHeader header;
	uint32_t const *p;
	uint32_t const *end;
	uint32_t avail_mask, mask, result;
	unsigned mbs_trigger, last_mod;
	int sam_i;

	LOGF(spam)(LOGL, NAME" parse_data {");
	result = CRATE_READOUT_FAIL_DATA_CORRUPT;
	tacquila = (void *)a_client;
	sam_i = tacquila->sam->address >> 24;
	p = a_event_buffer->ptr;
	header.u32 = *p++;
	if (sam_i != header.bits.sam) {
		log_error(LOGL, "Header (%08x) SAM ID %d != conf ID %d.",
		    header.u32, header.bits.sam, sam_i);
		goto gsi_tacquila_parse_data_done;
	}
	if (a_client->gtb_i != header.bits.gtb) {
		log_error(LOGL, "Header (%08x) GTB ID %d != conf ID %d.",
		    header.u32, header.bits.gtb, a_client->gtb_i);
		goto gsi_tacquila_parse_data_done;
	}
	if (tacquila->lec != header.bits.lec) {
		log_error(LOGL, "Header (%08x) lec %d != expected %d.",
		    header.u32, header.bits.lec, tacquila->lec);
		goto gsi_tacquila_parse_data_done;
	}
	mbs_trigger = crate_gsi_mbs_trigger_get(a_crate);
	if (mbs_trigger != header.bits.trg_sam) {
		log_error(LOGL, "Header (%08x) SAM trigger %d != expected "
		    "%d.", header.u32, header.bits.trg_sam, mbs_trigger);
		goto gsi_tacquila_parse_data_done;
	}
#if 0
	if (mbs_trigger != header.bits.trg_tacq) {
		log_error(LOGL, "Tacquila trigger %d != expected %d.",
		    header.bits.trg_tacq, mbs_trigger);
		goto gsi_tacquila_parse_data_done;
	}
#endif
	/* Don't count the SAM header. */
	if (a_event_buffer->bytes / sizeof(uint32_t) - 1 <
	    (size_t)header.bits.count) {
		log_error(LOGL, "SAM word count %d > expected total %"PRIz".",
		    header.bits.count, a_event_buffer->bytes /
		    sizeof(uint32_t) - 1);
		goto gsi_tacquila_parse_data_done;
	}
	avail_mask = 0;
	last_mod = 0;
	end = p + header.bits.count;
	for (; end > p;) {
		union TacquilaData data;
		unsigned cur_mod;

		data.u32 = *p++;
		cur_mod = data.bits.module;
		if (cur_mod < 1 || tacquila->card_num < cur_mod) {
			log_error(LOGL, "Module %u != [1, %u].", cur_mod,
			    tacquila->card_num);
			goto gsi_tacquila_parse_data_done;
		}
		if (last_mod != cur_mod) {
			unsigned bit;

			bit = 1 << (cur_mod - 1);
			if (0 != (avail_mask & bit)) {
				log_error(LOGL, "Module %d appears twice.",
				    cur_mod);
				goto gsi_tacquila_parse_data_done;
			}
			avail_mask |= bit;
			last_mod = cur_mod;
		}
		if (16 < data.bits.channel) {
			log_error(LOGL, "Channel %d invalid.",
			    data.bits.channel);
			goto gsi_tacquila_parse_data_done;
		}
		/*if (mbs_trigger != data.bits.trigger) {
			log_error(LOGL, "Data trigger %d != expected %d.",
			    data.bits.trigger, mbs_trigger);
			goto gsi_tacquila_parse_data_done;
		}*/
	}
	mask = (1 << tacquila->card_num) - 1;
	if (mask != avail_mask) {
		log_error(LOGL, "Tacquila mask %08x != expected %08x, 17th "
		    "channel missing?", avail_mask, mask);
		goto gsi_tacquila_parse_data_done;
	}
	result = 0;
gsi_tacquila_parse_data_done:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p);
	tacquila->lec = (tacquila->lec + 1) & 15;
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}
