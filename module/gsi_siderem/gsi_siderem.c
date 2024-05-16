/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2021, 2023-2024
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

#include <module/gsi_siderem/internal.h>
#include <sched.h>
#include <module/gsi_sam/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/bits.h>
#include <util/syscall.h>

#define NAME "Gsi Siderem"

static void	free_(struct GsiSamGtbClient **);
static uint32_t	parse_data(struct Crate const *, struct GsiSamGtbClient *,
    struct EventConstBuffer *) FUNC_RETURNS;

void
free_(struct GsiSamGtbClient **a_client)
{
	FREE(*a_client);
}

void
gsi_siderem_crate_add(struct GsiSideremCrate *a_crate, struct GsiSamGtbClient
    *a_client)
{
	struct GsiSiderem *siderem;

	LOGF(verbose)(LOGL, NAME" crate_add {");
	siderem = (void *)a_client;
	siderem->crate = a_crate;
	TAILQ_INSERT_TAIL(&a_crate->list, siderem, next);
	LOGF(verbose)(LOGL, NAME" crate_add }");
}

void
gsi_siderem_crate_configure(struct GsiSideremCrate *a_crate, struct
    ConfigBlock *a_block)
{
	LOGF(verbose)(LOGL, NAME" crate_configure {");
	FREE(a_crate->data_path);
	FREE(a_crate->util_path);
	a_crate->data_path = strdup(config_get_string(a_block, KW_DATA_PATH));
	a_crate->util_path = strdup(config_get_string(a_block, KW_UTIL_PATH));
	LOGF(verbose)(LOGL, "data_path = \"%s\".", a_crate->data_path);
	LOGF(verbose)(LOGL, "util_path = \"%s\".", a_crate->util_path);
	LOGF(verbose)(LOGL, NAME" crate_configure }");
}

void
gsi_siderem_crate_create(struct GsiSideremCrate *a_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_create {");
	a_crate->data_path = strdup(".");
	a_crate->util_path = strdup(".");
	TAILQ_INIT(&a_crate->list);
	LOGF(verbose)(LOGL, NAME" crate_create }");
}

void
gsi_siderem_crate_destroy(struct GsiSideremCrate *a_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_destroy {");
	FREE(a_crate->data_path);
	FREE(a_crate->util_path);
	LOGF(verbose)(LOGL, NAME" crate_destroy }");
}

int
gsi_siderem_crate_init_slow(struct GsiSideremCrate *a_crate)
{
	struct GsiSiderem *siderem;
	unsigned sam_mask;
	int ret;

	if (TAILQ_EMPTY(&a_crate->list)) {
		return 1;
	}

	LOGF(verbose)(LOGL, NAME" crate_init_slow {");
	ret = 0;

	TAILQ_FOREACH(siderem, &a_crate->list, next) {
		size_t ofs;

		ofs = GSI_SAM_GTB_OFFSET(siderem->sam,
		    siderem->gtb_client.gtb_i);
		MAP_WRITE_OFS(siderem->sam->crate->sicy_map, init, ofs, 0);
		siderem->lec = 0;
	}

	sam_mask = 0;
	TAILQ_FOREACH(siderem, &a_crate->list, next) {
		unsigned sam_i, bit, i;

		sam_i = siderem->sam->address >> 24;
		bit = 1 << sam_i;
		if (0 != (bit & sam_mask)) {
			continue;
		}
		sam_mask |= bit;
		LOGF(info)(LOGL, "Initializing SAM=%u...", sam_i);
		for (i = 0; i < 3; ++i) {
			/*
			 * This is ugly, but for Linux on RIO4, one specific
			 * register write has to pass before we can at all
			 * touch anything else on the SAM.
			 */
			system_call("%s/hstart %u", a_crate->util_path,
			    sam_i);
		}
		system_call("%s/paraload2s 0 %s/sidped_%u.txt",
		    a_crate->util_path, a_crate->data_path, sam_i);
		system_call("%s/paraload2s 1 %s/sidsig_%u.txt",
		    a_crate->util_path, a_crate->data_path, sam_i);
		system_call("%s/paraload2s 2 %s/sidsig_r_%u.txt",
		    a_crate->util_path, a_crate->data_path, sam_i);
		system_call("%s/sideload2 %u %s/readout.m0",
		    a_crate->util_path, sam_i, a_crate->data_path);
		system_call("%s/hpistart2 %u %s/sidesam5_sdram.m0",
		    a_crate->util_path, sam_i, a_crate->data_path);
	}

	TAILQ_FOREACH(siderem, &a_crate->list, next) {
		size_t ofs;

		ofs = GSI_SAM_GTB_OFFSET(siderem->sam,
		    siderem->gtb_client.gtb_i);
		for (;;) {
			uint32_t init;

			init = MAP_READ_OFS(siderem->sam->crate->sicy_map,
			    init, ofs);
			init &= 0xff;
			if (siderem->mod_num == init) {
				break;
			}
			if (0 != init) {
				log_error(LOGL, "SAM5 %u GTB %u: init "
				    "status=0x%02x, but 0x%02x modules "
				    "configured.",
				    siderem->sam->address >> 24,
				    siderem->gtb_client.gtb_i, init,
				    siderem->mod_num);
				goto gsi_siderem_crate_init_slow_done;
			}
			sched_yield();
		}
	}
	ret = 1;

gsi_siderem_crate_init_slow_done:
	LOGF(verbose)(LOGL, NAME" crate_init_slow }");
	return ret;
}

struct GsiSamGtbClient *
gsi_siderem_create(struct GsiSamModule *a_sam, struct ConfigBlock *a_block)
{
	struct GsiSiderem *siderem;
	int gtb_i;

	LOGF(verbose)(LOGL, NAME" create {");
	CALLOC(siderem, 1);
	gtb_i = config_get_block_param_int32(a_block, 0);
	if (!IN_RANGE_II(gtb_i, 0, 1)) {
		log_die(LOGL, NAME" create: GTB=%u should be 0 or 1.",
		    gtb_i);
	}
	GSI_SAM_GTB_CLIENT_CREATE(siderem->gtb_client, KW_GSI_SIDEREM, gtb_i,
	    free_, parse_data, NULL);
	siderem->sam = a_sam;
	siderem->mod_num = config_get_block_param_int32(a_block, 1);
	if (!IN_RANGE_II(siderem->mod_num, 1, 32)) {
		log_die(LOGL, NAME" create: Card-num=%u does not seem "
		    "right.", siderem->mod_num);
	}
	siderem->mod_mask = BITS_MASK_TOP(siderem->mod_num - 1);
	siderem->is_compressed = config_get_boolean(a_block, KW_COMPRESSED);

	LOGF(verbose)(LOGL, NAME" create }");
	return (void *)siderem;
}

uint32_t
parse_data(struct Crate const *a_crate, struct GsiSamGtbClient *a_client,
    struct EventConstBuffer *a_event_buffer)
{
	struct GsiSiderem *siderem;
	uint32_t const *p32;
	uint32_t header, result;
	unsigned mod_i, gtb_i, lec, sam_i, siderem_sam_i, sid_i, trigger, type_trigger;

	LOGF(spam)(LOGL, NAME" parse_data {");
	result = CRATE_READOUT_FAIL_DATA_CORRUPT;
	siderem = (void *)a_client;
	siderem_sam_i = siderem->sam->address >> 24;
	p32 = a_event_buffer->ptr;

	if (siderem->is_compressed) {
		type_trigger = 1 == crate_gsi_mbs_trigger_get(a_crate) ? 1 : 2;
	} else {
		type_trigger = 2;
	}

	for (mod_i = 0; mod_i < siderem->mod_num; ++mod_i) {
		header = *p32;
		sam_i = 0xf & (header >> 28);
		if (0 && siderem_sam_i != sam_i) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "SAM data ID %u != conf ID %u.", sam_i,
			    siderem_sam_i);
			goto gsi_siderem_parse_data_done;
		}
		gtb_i = 0xf & (header >> 24);
		if (a_client->gtb_i != gtb_i) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "GTB data ID %u != conf ID %u.", gtb_i,
			    a_client->gtb_i);
			goto gsi_siderem_parse_data_done;
		}
		sid_i = 0xf & (header >> 20);
		if (0 == (siderem->mod_mask & (1 << (sid_i - 1)))) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Siderem data ID %u (1-based) not in conf mask "
			    "0x%08x.", sid_i, siderem->mod_mask);
			goto gsi_siderem_parse_data_done;
		}
		trigger = 0xf & (header >> 16);
		if (trigger != type_trigger) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "Siderem trigger in data %u != expected trigger "
			    "%u (%s).", trigger, type_trigger, 1 ==
			    type_trigger ? "compressed" : "raw");
			goto gsi_siderem_parse_data_done;
		}
		lec = 0xf & (header >> 12);
		if (siderem->lec != lec) {
			module_parse_error(LOGL, a_event_buffer, p32,
			    "lec %u != expected %u.", lec, siderem->lec);
			goto gsi_siderem_parse_data_done;
		}

		p32 += 1 + (0xfff & header);
/* TODO: Check the advancing before dying! */
		EVENT_BUFFER_ADVANCE(*a_event_buffer, p32);
	}
	siderem->lec = 0xf & (1 + siderem->lec);
	result = 0;
gsi_siderem_parse_data_done:
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}
