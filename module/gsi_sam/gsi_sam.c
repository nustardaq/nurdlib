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

#include <module/gsi_sam/gsi_sam.h>
#include <sys/time.h>
#include <assert.h>
#include <sched.h>
#include <stddef.h>
#include <module/gsi_sam/internal.h>
#include <module/gsi_sam/offsets.h>
#include <module/gsi_siderem/internal.h>
#include <module/gsi_tacquila/internal.h>
#include <module/pnpi_cros3/internal.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/assert.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/time.h>

#include <nurdlib/serialio.h>

#define NAME "Gsi Sam"

#define DMA_FILLER 0x5a5a5a5a
#define READOUT_TIMEOUT 1.0
#define SICY_DMA_BREAK_EVEN 24

MODULE_PROTOTYPES(gsi_sam);

uint32_t
gsi_sam_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

void
gsi_sam_crate_collect(struct GsiSamCrate *a_sam_crate, struct GsiSideremCrate
    *a_siderem_crate, struct GsiTacquilaCrate *a_tacquila_crate, struct
    PnpiCros3Crate *a_cros3_crate, struct Module *a_module)
{
	struct GsiSamModule *sam;
	size_t gtb_i;
	unsigned id;

	LOGF(verbose)(LOGL, NAME" crate_collect {");
	MODULE_CAST(KW_GSI_SAM, sam, a_module);
	if (KW_BLT != a_sam_crate->blt_mode &&
	    KW_MBLT != a_sam_crate->blt_mode) {
		a_sam_crate->blt_mode = sam->blt_mode;
	} else if (a_sam_crate->blt_mode != sam->blt_mode) {
		log_die(LOGL, "Nurdlib only supports one BLT mode in one "
		    "crate!");
	}
	id = sam->address >> 24;
	a_sam_crate->first = MIN(a_sam_crate->first, id);
	a_sam_crate->last = MAX(a_sam_crate->last, id);
	for (gtb_i = 0; LENGTH(sam->gtb_client) > gtb_i; ++gtb_i) {
		struct GsiSamGtbClient *client;

		client = sam->gtb_client[gtb_i];
		if (NULL == client) {
			continue;
		}
		if (KW_GSI_SIDEREM == client->type) {
			gsi_siderem_crate_add(a_siderem_crate, client);
		} else if (KW_GSI_TACQUILA == client->type) {
			gsi_tacquila_crate_add(a_tacquila_crate, client);
		} else if (KW_PNPI_CROS3 == client->type) {
			pnpi_cros3_crate_add(a_cros3_crate, client);
		}
	}
	sam->crate = a_sam_crate;
	LOGF(verbose)(LOGL, NAME" crate_collect }");
}

void
gsi_sam_crate_create(struct GsiSamCrate *a_sam_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_create {");
	a_sam_crate->blt_mode = KW_NONE;
	a_sam_crate->first = 0xff;
	a_sam_crate->last = 0;
	a_sam_crate->sicy_map = NULL;
	a_sam_crate->blt_map = NULL;
	LOGF(verbose)(LOGL, NAME" crate_create }");
}

void
gsi_sam_crate_deinit(struct GsiSamCrate *a_sam_crate)
{
	LOGF(info)(LOGL, NAME" crate_deinit {");
	map_unmap(&a_sam_crate->sicy_map);
	map_unmap(&a_sam_crate->blt_map);
	LOGF(info)(LOGL, NAME" crate_deinit }");
}

int
gsi_sam_crate_init_fast(struct GsiSamCrate *a_sam_crate)
{
	(void)a_sam_crate;
	return 1;
}

int
gsi_sam_crate_init_slow(struct GsiSamCrate *a_sam_crate)
{
	LOGF(info)(LOGL, NAME" crate_init_slow {");
	if ((0xff != a_sam_crate->first || 0 != a_sam_crate->last) &&
	    a_sam_crate->first > a_sam_crate->last) {
		log_die(LOGL, "SAM crate first=%u last=%u, bad!!!",
		    a_sam_crate->first, a_sam_crate->last);
	}
	assert(NULL == a_sam_crate->sicy_map);
	assert(NULL == a_sam_crate->blt_map);
	if (a_sam_crate->first <= a_sam_crate->last) {
		size_t size;

		size = (a_sam_crate->last + 1 - a_sam_crate->first) << 24;
		a_sam_crate->sicy_map = map_map(a_sam_crate->first << 24,
		    size, KW_NOBLT, 0, 0,
		    3, 0x00400000, 32,
		    0, 0, 0, 0);
		if (KW_NONE != a_sam_crate->blt_mode) {
			a_sam_crate->blt_map = map_map(a_sam_crate->first <<
			    24, size, a_sam_crate->blt_mode, 0, 0,
			    3, 0x00400000, 32,
			    0, 0, 0, 0);
		}
	}
	LOGF(info)(LOGL, NAME" crate_init_slow }");
	return 1;
}

struct Module *
gsi_sam_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	enum Keyword const c_blt_mode[] = {
		KW_BLT,
		KW_MBLT,
		KW_NOBLT
	};
	struct GsiSamModule *sam;
	struct ConfigBlock *client_block;
	unsigned siderem_num, tacquila_num, cros3_num;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(sam);
	sam->module.event_max = 1;

	sam->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", sam->address);

	sam->blt_mode = CONFIG_GET_KEYWORD(a_block, KW_BLT_MODE, c_blt_mode);
	LOGF(verbose)(LOGL, "Using BLT mode = %s.",
	    keyword_get_string(sam->blt_mode));

	ZERO(sam->gtb_client);
#define PARSE_CLIENTS(Type, creator, counter)\
	do {\
		counter = 0;\
		for (client_block = config_get_block(a_block, Type);\
		    NULL != client_block;\
		    client_block = config_get_block_next(client_block, Type))\
		{\
			struct GsiSamGtbClient *client;\
\
			client = creator(sam, client_block);\
			client->type = Type;\
			ASSERT(size_t, PRIz, LENGTH(sam->gtb_client), >,\
			    client->gtb_i);\
			if (NULL != sam->gtb_client[client->gtb_i]) {\
				log_die(LOGL, "SAM GTB %d already used!",\
				    client->gtb_i);\
			}\
			sam->gtb_client[client->gtb_i] = client;\
			++counter;\
		}\
	} while (0)
	PARSE_CLIENTS(KW_GSI_SIDEREM, gsi_siderem_create, siderem_num);
	PARSE_CLIENTS(KW_GSI_TACQUILA, gsi_tacquila_create, tacquila_num);
	PARSE_CLIENTS(KW_PNPI_CROS3, pnpi_cros3_create, cros3_num);
	if (1 < (0 != siderem_num) + (0 != tacquila_num) + (0 != cros3_num)) {
		log_die(LOGL, "Mixing GTB clients in one SAM prohibited "
		    "(tacq=%d cros3=%d)!", tacquila_num, cros3_num);
	}

	LOGF(verbose)(LOGL, NAME" create(sid=%u,tac=%u,cros3=%u) }",
	    siderem_num, tacquila_num, cros3_num);

	return (void *)sam;
}

void
gsi_sam_deinit(struct Module *a_module)
{
	(void)a_module;
}

void
gsi_sam_destroy(struct Module *a_module)
{
	struct GsiSamModule *sam;
	size_t gtb_i;

	LOGF(verbose)(LOGL, NAME" destroy {");
	MODULE_CAST(KW_GSI_SAM, sam, a_module);
	for (gtb_i = 0; LENGTH(sam->gtb_client) > gtb_i; ++gtb_i) {
		struct GsiSamGtbClient *client;

		client = sam->gtb_client[gtb_i];
		if (NULL == client) {
			continue;
		}
		client->free(&sam->gtb_client[gtb_i]);
	}
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
gsi_sam_get_map(struct Module *a_module)
{
	(void)a_module;
	return NULL;
}

void
gsi_sam_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	/* TODO: Payload signature depends on the clients. */
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
gsi_sam_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	(void)a_module;
	return 1;
}

int
gsi_sam_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiSamModule *sam;
	size_t gtb_i;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_GSI_SAM, sam, a_module);
	for (gtb_i = 0; LENGTH(sam->gtb_client) > gtb_i; ++gtb_i) {
		struct GsiSamGtbClient *client;

		client = sam->gtb_client[gtb_i];
		if (NULL == client) {
			continue;
		}
	}
	LOGF(info)(LOGL, NAME" init_slow }");
	return 1;
}

void
gsi_sam_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_sam_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct EventConstBuffer ebuf;
	struct GsiSamModule *sam;
	void const *p;
	uint32_t result;
	size_t gtb_i;

	(void)a_do_pedestals;
	LOGF(spam)(LOGL, NAME" parse_data {");
	MODULE_CAST(KW_GSI_SAM, sam, a_module);
	result = 0;
	p = a_event_buffer->ptr;
	COPY(ebuf, *a_event_buffer);
	for (gtb_i = 0; LENGTH(sam->gtb_client) > gtb_i; ++gtb_i) {
		struct GsiSamGtbClient *client;
		uint32_t const *p32;
		uint32_t ret;

		client = sam->gtb_client[gtb_i];
		if (NULL == client) {
			continue;
		}
		if (NULL != client->rewrite_data) {
			goto gsi_sam_parse_data_done;
		}
		p32 = ebuf.ptr;
		if (DMA_FILLER == *p32) {
			++p32;
			EVENT_BUFFER_ADVANCE(ebuf, p32);
		}
		ret = client->parse_data(a_crate, client, &ebuf);
		if (0 != ret) {
			log_error(LOGL, "SAM=%d GTB=%"PRIz": parse error.",
			    sam->address >> 24, gtb_i);
		}
		result |= ret;
	}
	if (0 == result && 0 != ebuf.bytes) {
		module_parse_error(LOGL, a_event_buffer, p,
		    "Data left after GTB client parsing");
		result = CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	}
gsi_sam_parse_data_done:
	LOGF(spam)(LOGL, NAME" parse_data(0x%08x) }", result);
	return result;
}

/*
static double g_inner, g_outer;
static uint32_t g_counter, g_polls;
double t_1, t_2;
*/

uint32_t
gsi_sam_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiSamModule *sam;
	uint32_t *outp;
	uint32_t result = 0;
	size_t gtb_i;
/*	unsigned event_no;*/

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout {");

	MODULE_CAST(KW_GSI_SAM, sam, a_module);

/*
	for (gtb_i = 0; LENGTH(sam->gtb_client) > gtb_i; ++gtb_i) {
		struct GsiSamGtbInterface volatile *proto;
		double t, time_wait;

		if (NULL == sam->gtb_client[gtb_i]) {
			continue;
		}
		proto = sam->gtb_iface[gtb_i];
		for (time_wait = -1;;) {
			SERIALIZE_IO;
			if (1 == proto->prot) {
				break;
			}
			sched_yield();
			t = time_getd();
			if (0 > time_wait) {
				time_wait = t + READOUT_TIMEOUT;
			}
			if (t > time_wait) {
				log_error(LOGL, "SAM=%d GTB=%"PRIz": data "
				    "timeout (>%fs).", sam->address >> 24,
				    gtb_i, READOUT_TIMEOUT);
				result = CRATE_READOUT_FAIL_DATA_MISSING;
				goto gsi_sam_readout_done;
			}
		}
	}
*/

/*
t_1 = time_getd();
if (t_2) g_outer += t_1 - t_2;
*/
	outp = a_event_buffer->ptr;
/*	event_no = -1;*/
	for (gtb_i = 0; LENGTH(sam->gtb_client) > gtb_i; ++gtb_i) {
		struct GsiSamGtbClient const *client;
		size_t data_words, ofs;
/*		unsigned cur_event_no;*/

		client = sam->gtb_client[gtb_i];
		if (NULL == client) {
			continue;
		}
		ofs = GSI_SAM_GTB_OFFSET(sam, gtb_i);
		data_words = MAP_READ_OFS(sam->crate->sicy_map, size, ofs)
		    / 4 - 2;
		if (a_event_buffer->bytes < sizeof(uint32_t) * data_words) {
			result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto gsi_sam_readout_done;
		}
		/* TODO: Check rest of proto? */
		if (KW_NOBLT == sam->blt_mode ||
		    SICY_DMA_BREAK_EVEN >= data_words) {
			size_t i;

			for (i = 0; data_words > i; i++) {
				*outp++ = MAP_READ_OFS(sam->crate->sicy_map,
				    buf(i), ofs);
			}
		} else {
			uintptr_t bytes;
			int ret;

			assert(NULL != sam->crate->blt_map);
			bytes = sizeof(uint32_t) * data_words;
			outp = map_align(outp, &bytes, sam->crate->blt_mode,
			    DMA_FILLER);
			ret = map_blt_read(sam->crate->blt_map,
			    GSI_SAM_GTB_OFFSET(sam, gtb_i) + OFS_buf(0), outp,
			    bytes);
			if (-1 == ret) {
				log_error(LOGL, NAME "Wanted to "
				    "read=%"PRIpx"B, read=%uB.", bytes, ret);
				result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
				goto gsi_sam_readout_done;
			}
			outp += data_words;
		}
		MAP_WRITE_OFS(sam->crate->sicy_map, prot, ofs, 0);
		if (NULL != client->rewrite_data) {
			struct EventBuffer ebuf;

			ebuf.ptr = a_event_buffer->ptr;
			ebuf.bytes = data_words * sizeof(uint32_t);
			result |= client->rewrite_data(client, &ebuf);
			outp = ebuf.ptr;
		}
		EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);

#if 0
/* This doesn't work here because we release DT just above! */
		/* Check that all SAMs have identical event_no. */
		cur_event_no = proto->event_no;
		if ((unsigned)-1 == event_no) {
			event_no = cur_event_no;
		} else if (cur_event_no != event_no) {
			log_error(LOGL, "Event no mismatch, cur = %d, prev ="
			    " %d.", cur_event_no, event_no);
			goto gsi_sam_readout_done;
		}
#endif
	}
/*
	t_2 = time_getd();
	g_inner += t_2 - t_1;
	++g_counter;
	if (0 == g_counter % 200) {
	printf("%g %g %g\n", g_inner / g_counter, g_outer / g_counter, (double)g_polls / g_counter);
	}
*/
gsi_sam_readout_done:
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

uint32_t
gsi_sam_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiSamModule *sam;
	size_t gtb_i;
	uint32_t result = 0;
	int had_to_wait;

	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_GSI_SAM, sam, a_module);

	if (crate_gsi_mbs_trigger_get(a_crate) > 3) {
		goto gsi_sam_readout_dt_done;
	}

	/* Wait for busy release + data on all used GTBs. */
	had_to_wait = 0;
	for (gtb_i = 0; LENGTH(sam->gtb_client) > gtb_i; ++gtb_i) {
		double t, time_wait;
		size_t ofs;

		if (NULL == sam->gtb_client[gtb_i]) {
			continue;
		}
		ofs = GSI_SAM_GTB_OFFSET(sam, gtb_i);
		if (KW_PNPI_CROS3 != sam->gtb_client[gtb_i]->type) {
			for (time_wait = -1;;) {
				SERIALIZE_IO;
				if (1 == MAP_READ_OFS(sam->crate->sicy_map,
				    trcl, ofs)) {
					break;
				}
				sched_yield();
				t = time_getd();
				if (0 > time_wait) {
					time_wait = t + READOUT_TIMEOUT;
				} else if (t > time_wait) {
					log_error(LOGL, "SAM=%d GTB=%"PRIz":"
					    " busy timeout (>%fs).",
					    sam->address >> 24, gtb_i,
					    READOUT_TIMEOUT);
					result =
					    CRATE_READOUT_FAIL_DATA_MISSING;
					goto gsi_sam_readout_dt_done;
				}
				had_to_wait = 1;
			}
			MAP_WRITE_OFS(sam->crate->sicy_map, trcl, ofs, 0);
		}
		for (time_wait = -1;;) {
			SERIALIZE_IO;
			if (1 == MAP_READ_OFS(sam->crate->sicy_map, prot,
			    ofs)) {
				break;
			}
			sched_yield();
			t = time_getd();
			if (0 > time_wait) {
				time_wait = t + READOUT_TIMEOUT;
			}
			if (t > time_wait) {
				log_error(LOGL, "SAM=%d GTB=%"PRIz": data "
				    "timeout (>%fs).", sam->address >> 24,
				    gtb_i, READOUT_TIMEOUT);
				result = CRATE_READOUT_FAIL_DATA_MISSING;
				goto gsi_sam_readout_dt_done;
			}
		}
	}
	if (had_to_wait) {
		crate_acvt_grow(a_crate);
	}
	if (1 || 0 != MODULE_COUNTER_DIFF(*a_module)) {
		++a_module->event_counter.value;
	}
gsi_sam_readout_dt_done:

	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x,result=0x%08x) }",
	    a_module->event_counter.value, result);
	return result;
}

void
gsi_sam_setup_(void)
{
	MODULE_SETUP(gsi_sam, MODULE_FLAG_EARLY_DT);
}
