/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2019, 2021-2022, 2024
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

#include <module/pnpi_cros3/internal.h>
#include <errno.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/bits.h>
#include <util/assert.h>
#include <util/fmtmod.h>
#include <util/math.h>
#include <util/string.h>
#include <util/syscall.h>

#define NAME "Pnpi Cros3"

static void	free_(struct GsiSamGtbClient **);
static char	*para_write(struct PnpiCros3Crate const *, struct PnpiCros3
    const *) FUNC_RETURNS;
static uint32_t	parse_data(struct Crate const *, struct GsiSamGtbClient *,
    struct EventConstBuffer *) FUNC_RETURNS;
static uint32_t	rewrite_data(struct GsiSamGtbClient const *, struct
    EventBuffer *) FUNC_RETURNS;

void
free_(struct GsiSamGtbClient **a_client)
{
	FREE(*a_client);
}

char *
para_write(struct PnpiCros3Crate const *a_crate, struct PnpiCros3 const
    *a_cros3)
{
	FILE *file;
	char sam_str[4]; /* Leave enough room for snprintf */
	char gtb_str[3];
	char *path;

	/* TODO: Check snprintf result */
	snprintf_(sam_str, sizeof sam_str, "%u", a_cros3->sam->address >> 24);
	snprintf_(gtb_str, sizeof gtb_str, "%1u", a_cros3->gtb_client.gtb_i);
	path = NULL;
	path = STRCTV_BEGIN &path, a_crate->data_path, "/pnpi_cros3_sam",
	     sam_str, "_gtb", gtb_str, ".txt" STRCTV_END;
	file = fopen(path, "wb");
	if (NULL == file) {
		log_die(LOGL, "fopen(%s, wb): %s", path, strerror(errno));
	}
	fprintf(file, "%u %u\n", a_cros3->ccb_id, a_cros3->gtb_client.gtb_i);
	fprintf(file, "w 1 0 act_res 0\n");
	fprintf(file, "w 1 0 act_ini 0\n");
	fprintf(file, "r 1 0 csr_ini\n");
	fprintf(file, "w 1 0 csr_cid %u\n", a_cros3->ccb_id);
	fprintf(file, "w 1 0 csr_den ffff\n");
#define LOOP_CSR(name, digit_num)\
	do {\
		unsigned i;\
		for (i = 0; a_cros3->card_num > i; ++i) {\
			fprintf(file, "w 0 %x csr_"#name" %0"#digit_num"x\n",\
			    i, a_cros3->regs.name);\
		}\
	} while (0)
	LOOP_CSR(ddc, 2);
	LOOP_CSR(drc, 3);
	LOOP_CSR(thc, 3);
	LOOP_CSR(thl, 2);
	fprintf(file, "r 0 f csr_thl\n");
	LOOP_CSR(thu, 2);
	fprintf(file, "r 0 f csr_thu\n");
	LOOP_CSR(tpe, 2);
	LOOP_CSR(tpo, 2);
	LOOP_CSR(trc, 3);
	LOOP_CSR(wim, 4);
	fprintf(file, "w 1 0 csr_trc 0\n");
	fclose(file);
	return path;
}

uint32_t
parse_data(struct Crate const *a_crate, struct GsiSamGtbClient *a_client,
    struct EventConstBuffer *a_event_buffer)
{
	uint16_t const *p;
	unsigned i, result;
	int trigger_config;

	(void)a_crate;
	(void)a_client;
	LOGF(spam)(LOGL, NAME" parse_data {");
	result = CRATE_READOUT_FAIL_DATA_CORRUPT;

	p = a_event_buffer->ptr;
#define CHECK_ACCESS(ptr_)\
	do {\
		if (!MEMORY_CHECK(*a_event_buffer, ptr_)) {\
			module_parse_error(LOGL, a_event_buffer->ptr,\
			    ptr_, "Unexpected end of data");\
			goto pnpi_cros3_parse_data_done;\
		}\
	} while (0)
#define CHECK_BITS(name, loc, mask, value)\
	do {\
		if (value != (mask & loc)) {\
			module_parse_error(LOGL, a_event_buffer->ptr,\
			    &loc, #name" (0x%04x & 0x%04x) != 0x%04x", mask,\
			    loc, value);\
			goto pnpi_cros3_parse_data_done;\
		}\
	} while (0)
	CHECK_ACCESS(&p[3]);
	CHECK_BITS("header", p[0], 0xff00, 0xab00);
	for (i = 1; 4 > i; ++i) {
		CHECK_BITS("header", p[i], 0xf000, 0xa000);
	}
	p += 4;
	trigger_config = -1;
	for (;;) {
		unsigned slice_num;
		int trig_conf;

		CHECK_ACCESS(p);
		if (0xde00 == (0xff00 & *p)) {
			break;
		}
		CHECK_ACCESS(&p[3]);
		for (i = 0; 4 > i; ++i) {
			CHECK_BITS("AD16 header", p[i], 0xf000, 0xc000);
		}
		trig_conf = 0x0003 & p[0];
		if (-1 == trigger_config) {
			trigger_config = trig_conf;
		} else if (trigger_config != trig_conf) {
			module_parse_error(LOGL, a_event_buffer->ptr, p,
			    "Different trigger configs for different AD16 "
			    "cards (cur=%d prev=%d)", trig_conf,
			    trigger_config);
			goto pnpi_cros3_parse_data_done;
		}
		p += 4;

		switch (trigger_config) {
		case 0: /* Threshold mode. */
			/* TODO: Use these. */
			/*thr_step = (0x0f00 & p[-3]) >> 8;
			thr_begin = 0x00ff & p[-3];*/
			for (;;) {
				unsigned ch_i;

				CHECK_ACCESS(p);
				/* TODO: This is not a pretty condition. */
				if (0x0000 != (0xf000 &*p)) {
					break;
				}
				CHECK_ACCESS(&p[15]);
				for (ch_i = 0; 16 > ch_i; ++ch_i, ++p) {
					unsigned ch;

					ch = *p >> 12;
					if (ch != ch_i) {
						module_parse_error(LOGL,
						    a_event_buffer->ptr, p,
						    "Got ch %u, expected %u",
						    ch, ch_i);
						goto
						    pnpi_cros3_parse_data_done;
					}
				}
			}
			break;
		case 1: /* Raw ToT. */
		case 2: /* Raw leading. */
			slice_num = 0x00ff & p[-3];
			CHECK_ACCESS(&p[slice_num - 1]);
			p += slice_num;
			break;
		case 3: /* Encoded leading. */
			for (;; ++p) {
				CHECK_ACCESS(p);
				if (0 != (0xf000 & *p) && 0x1000 != (0xf000 &
				    *p)) {
					break;
				}
			}
			break;
		default:
			assert(0 && "This cannot happen.");
		}
	}
	CHECK_ACCESS(&p[1]);
	CHECK_BITS("footer 0", p[0], 0xff00, 0xde00);
	CHECK_BITS("footer 1", p[1], 0xff00, 0xde00);
	p += 2;
	/* 32-bit alignment. */
	p += 1 & ((uintptr_t)p >> 1);
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p);
	result = 0;
pnpi_cros3_parse_data_done:
	LOGF(spam)(LOGL, NAME" parse_data(0x%08x) }", result);
	return result;
}

void
pnpi_cros3_crate_add(struct PnpiCros3Crate *a_crate, struct GsiSamGtbClient
    *a_client)
{
	struct PnpiCros3 *cros3;

	LOGF(verbose)(LOGL, NAME" crate_add {");
	cros3 = (void *)a_client;
	TAILQ_INSERT_TAIL(&a_crate->list, cros3, next);
	LOGF(verbose)(LOGL, NAME" crate_add }");
}

void
pnpi_cros3_crate_create(struct PnpiCros3Crate *a_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_create {");
	a_crate->data_path = NULL;
	TAILQ_INIT(&a_crate->list);
	LOGF(verbose)(LOGL, NAME" crate_create }");
}

void
pnpi_cros3_crate_destroy(struct PnpiCros3Crate *a_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_destroy {");
	FREE(a_crate->data_path);
	LOGF(verbose)(LOGL, NAME" crate_destroy }");
}

int
pnpi_cros3_crate_init_slow(struct PnpiCros3Crate *a_crate)
{
	struct PnpiCros3 *cros3;
	unsigned sam_mask;
	int ret;

	if (TAILQ_EMPTY(&a_crate->list)) {
		return 1;
	}

	LOGF(info)(LOGL, NAME" crate_init_slow {");
	ret = 0;

	sam_mask = 0;
	TAILQ_FOREACH(cros3, &a_crate->list, next) {
		char *path;
		unsigned sam_i, bit;

		sam_i = cros3->sam->address >> 24;
		bit = 1 << sam_i;
		if (0 == (bit & sam_mask)) {
			LOGF(info)(LOGL, "Initializing SAM=%u...", sam_i);
			if (!system_call("%s/hstart %u", a_crate->data_path,
			    sam_i)) {
				goto pnpi_cros3_crate_init_slow_done;
			}
			sam_mask |= bit;
		}

		LOGF(info)(LOGL, "Initializing SAM=%u GTB=%u...", sam_i,
		    cros3->gtb_client.gtb_i);
		path = para_write(a_crate, cros3);
		/* TODO: Check if the following SIGBUS:es. */
		if (!system_call("%s/paraload %u %s", a_crate->data_path,
		    sam_i, path)) {
			goto pnpi_cros3_crate_init_slow_done;
		}
		FREE(path);
		if (!system_call("%s/hpiload %u %u %s/cros3read.m0",
		    a_crate->data_path, sam_i, cros3->gtb_client.gtb_i,
		    a_crate->data_path)) {
			goto pnpi_cros3_crate_init_slow_done;
		}
	}
	ret = 1;

pnpi_cros3_crate_init_slow_done:
	LOGF(info)(LOGL, NAME" crate_init_slow }");
	return ret;
}

void
pnpi_cros3_crate_configure(struct PnpiCros3Crate *a_crate, struct ConfigBlock
    *a_block)
{
	LOGF(info)(LOGL, NAME" crate_configure {");
	FREE(a_crate->data_path);
	a_crate->data_path = strdup_(config_get_string(a_block,
	    KW_DATA_PATH));
	LOGF(info)(LOGL, "data_path = '%s'.", a_crate->data_path);
	LOGF(info)(LOGL, NAME" crate_configure }");
}

struct GsiSamGtbClient *
pnpi_cros3_create(struct GsiSamModule *a_sam, struct ConfigBlock *a_block)
{
	struct PnpiCros3 *cros3;
	unsigned gtb_i;

	LOGF(verbose)(LOGL, NAME" create {");
	CALLOC(cros3, 1);
	gtb_i = config_get_block_param_int32(a_block, 0);
	if (gtb_i > 1) {
		log_die(LOGL, NAME" create: GTB=%u does not seem right.",
		    gtb_i);
	}
	GSI_SAM_GTB_CLIENT_CREATE(cros3->gtb_client, KW_PNPI_CROS3, gtb_i,
	    free_, parse_data, NULL);
	cros3->sam = a_sam;
	cros3->ccb_id = config_get_block_param_int32(a_block, 1);
	if (!IN_RANGE_II(cros3->ccb_id, 1, 16)) {
		log_die(LOGL, NAME" create: Card-num=%u does not seem "
		    "right.", cros3->ccb_id);
	}
	cros3->card_num = config_get_block_param_int32(a_block, 2);
	if (!IN_RANGE_II(cros3->card_num, 1, 256)) {
		log_die(LOGL, NAME" create: Card-num=%u does not seem "
		    "right.", cros3->card_num);
	}

	cros3->regs.ddc = config_get_double(a_block, KW_TIME_AFTER_TRIGGER,
	    CONFIG_UNIT_NS, 0, 2550) / 10;
	LOGF(verbose)(LOGL, "Trigger delay = 0x%02x.", cros3->regs.ddc);
	{
		double scale;

		scale = config_get_double(a_block, KW_RESOLUTION,
		    CONFIG_UNIT_NS, 2.5, 20);
		if (5 > scale) {
		} else if (10 > scale) {
			cros3->regs.drc |= 0x100;
		} else if (20 > scale) {
			cros3->regs.drc |= 0x200;
		} else {
			cros3->regs.drc |= 0x300;
		}
		cros3->regs.drc |= config_get_int32(a_block, KW_SLICE_NUM,
		    CONFIG_UNIT_NONE, 0, 0xff);
		LOGF(verbose)(LOGL, "Readout control = 0x%03x.",
		    cros3->regs.drc);
	}
	{
		int begin, step;

		begin = config_get_int32(a_block, KW_THRESHOLD_BEGIN,
		    CONFIG_UNIT_NONE, 0, 0xff);
		step = config_get_int32(a_block, KW_THRESHOLD_STEP,
		    CONFIG_UNIT_NONE, 0, 0xf);
		cros3->regs.thc |= step << 8 | begin;
	}
	{
		uint32_t threshold_array[2];

		CONFIG_GET_UINT_ARRAY(threshold_array, a_block, KW_THRESHOLD,
		    CONFIG_UNIT_NONE, 0, 0xff);
		cros3->regs.thl = threshold_array[0];
		cros3->regs.thu = threshold_array[1];
	}
	{
		uint32_t test_pulse_array[2];

		CONFIG_GET_UINT_ARRAY(test_pulse_array, a_block,
		    KW_TEST_PULSE, CONFIG_UNIT_NONE, 0, 0xff);
		cros3->regs.tpe = test_pulse_array[0];
		cros3->regs.tpo = test_pulse_array[1];
	}
	{
		enum Keyword c_mode[] = {KW_THRESHOLD, KW_RAW_TOT, KW_RAW,
			KW_ENCODED, KW_REWRITE};
		enum Keyword mode;
		int statistics;

		mode = CONFIG_GET_KEYWORD(a_block, KW_MODE, c_mode);
		if (KW_THRESHOLD == mode) {
		} else if (KW_RAW_TOT == mode || KW_REWRITE == mode) {
			cros3->regs.trc = 1;
		} else if (KW_RAW == mode) {
			cros3->regs.trc = 2;
		} else if (KW_ENCODED == mode) {
			cros3->regs.trc = 3;
		}
		if (KW_REWRITE == mode) {
			cros3->gtb_client.rewrite_data = rewrite_data;
		}
		if (config_get_boolean(a_block, KW_USE_TEST_PULSE)) {
			cros3->regs.trc |= 0x10;
		}
		statistics = config_get_int32(a_block, KW_STATISTICS,
		    CONFIG_UNIT_NONE, 32, 4096);
		statistics = floor(log(statistics - 1) / M_LN2) - 4;
		ASSERT(int, "d", 0, <=, statistics);
		ASSERT(int, "d", 7, >=, statistics);
		cros3->regs.trc |= statistics << 8;
	}
	cros3->regs.wim = ~config_get_bitmask(a_block, KW_CHANNEL_ENABLE, 0,
	    15);

	LOGF(verbose)(LOGL, NAME" create }");
	return (void *)cros3;
}

uint32_t
rewrite_data(struct GsiSamGtbClient const *a_client, struct EventBuffer
    *a_event_buffer)
{
	struct Hit {
		uint16_t	le;
		uint16_t	te;
	};
	struct Channel {
		struct	Hit hit_array[100];
	};
	struct AD16 {
		struct	Channel channel_array[16];
	};
	struct AD16 ad16_array[16];
	uint8_t hit_num[16][16];
	void *end;
	uint16_t const *r16;
	uint32_t *w32, *header;
	size_t ad16_i;
	uint32_t trigger_num, ccb_id, trigger_time;
	uint32_t test_pulse = -1, statistics = -1, slice_num = -1, scale = -1,
		 mode = -1;
	unsigned i, result;

	ASSERT(size_t, PRIz, LENGTH(hit_num), ==, LENGTH(ad16_array));
	ASSERT(size_t, PRIz, LENGTH(hit_num[0]), ==,
	    LENGTH(ad16_array[0].channel_array));
	ASSERT(size_t, PRIz, 255, >,
	    LENGTH(ad16_array[0].channel_array[0].hit_array));

	/* TODO: Use card_num. */
	(void)a_client;
	LOGF(spam)(LOGL, NAME" rewrite_data {");
	result = CRATE_READOUT_FAIL_DATA_CORRUPT;

	r16 = a_event_buffer->ptr;
	end = (uint8_t *)a_event_buffer->ptr + a_event_buffer->bytes;
#undef CHECK_ACCESS
#define CHECK_ACCESS(ptr_)\
	do {\
		if (!MEMORY_CHECK(*a_event_buffer, ptr_)) {\
			module_parse_error(LOGL, a_event_buffer->ptr, ptr_,\
			    "Unexpected end of data");\
			goto pnpi_cros3_rewrite_data_done;\
		}\
	} while (0)
#undef CHECK_BITS
#define CHECK_BITS(name, loc, mask, value)\
	do {\
		if (value != (mask & loc)) {\
			module_parse_error(LOGL, a_event_buffer->ptr, &loc,\
			    #name" (0x%04x & 0x%04x) != 0x%04x", mask, loc,\
			    value);\
			goto pnpi_cros3_rewrite_data_done;\
		}\
	} while (0)
	CHECK_ACCESS(&r16[3]);
	CHECK_BITS("header", r16[0], 0xff00, 0xab00);
	for (i = 1; 4 > i; ++i) {
		CHECK_BITS("header", r16[i], 0xf000, 0xa000);
	}
	trigger_time = BITS_GET(r16[1], 8, 11);
	ccb_id = BITS_GET(r16[2], 8, 11);
	trigger_num = BITS_GET(r16[3], 8, 11);
	r16 += 4;
	ZERO(hit_num);
	for (;;) {
		struct AD16 *ad16;
		size_t ch_i;
		uint32_t slice_i, trig_conf;
		uint16_t u16, prev_mask;

		CHECK_ACCESS(r16);
		if (0xde00 == (0xff00 & *r16)) {
			break;
		}
		CHECK_ACCESS(&r16[3]);
		for (i = 0; 4 > i; ++i) {
			CHECK_BITS("AD16 header", r16[i], 0xf000, 0xc000);
		}
		trig_conf = 0x0003 & r16[0];
		if (1 != trig_conf) {
			module_parse_error(LOGL, a_event_buffer->ptr, r16,
			    "Expected raw ToT trigger config (cur=%u!=1)",
			    trig_conf);
			goto pnpi_cros3_rewrite_data_done;
		}
		mode = BITS_GET(r16[0], 0, 1);
		test_pulse = BITS_GET(r16[0], 4, 4);
		statistics = BITS_GET(r16[0], 8, 10);
		slice_num = BITS_GET(r16[1], 0, 7);
		scale = BITS_GET(r16[1], 8, 9);
		ad16_i = BITS_GET(r16[2], 8, 11);
		r16 += 4;

		/* Collect edges for one AD16. */
		ad16 = &ad16_array[ad16_i];
		/*
		 * Check next AD16 header/ CROS3 footer, simplifies
		 * 0-suppression.
		 */
		CHECK_ACCESS(&r16[slice_num]);
		u16 = r16[slice_num];
		if (0xc000 != (0xf000 & u16) && 0xde00 != (0xff00 & u16)) {
			module_parse_error(LOGL, a_event_buffer->ptr, r16,
			    "Expected AD16 header or CROS3 footer after "
			    "slices, got 0x%04x", u16);
			goto pnpi_cros3_rewrite_data_done;
		}
		prev_mask = *r16++;
		for (ch_i = 0; LENGTH(ad16->channel_array) > ch_i; ++ch_i) {
			struct Channel *channel;

			channel = &ad16->channel_array[ch_i];
			channel->hit_array[0].le = 255;
		}
		for (slice_i = 1; slice_num > slice_i; ++slice_i) {
			uint16_t mask, d;

			mask = *r16++;
			d = mask ^ prev_mask;
			prev_mask = mask;
			if (0 == d) {
				/* No channel changed for this slice. */
				/* Speedy Gonzeros. */
				if (0 == mask) {
					for (; 0 == *r16; ++r16, ++slice_i)
						;
				}
				continue;
			}
			for (ch_i = 0; LENGTH(ad16->channel_array) > ch_i;
			    ++ch_i) {
				struct Channel *channel;
				uint8_t *hn;
				unsigned bit;

				bit = 1 << ch_i;
				if (0 == (d & bit)) {
					/* This channel didn't change. */
					continue;
				}
				hn = &hit_num[ad16_i][ch_i];
				channel = &ad16->channel_array[ch_i];
				if (LENGTH(channel->hit_array) <= *hn) {
					result =
					    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
					goto pnpi_cros3_rewrite_data_done;
				}
				if (mask & bit) {
					channel->hit_array[*hn].le = slice_i;
					channel->hit_array[*hn].le = 0;
				} else {
					channel->hit_array[*hn].te = slice_i;
					++*hn;
					channel->hit_array[*hn].le = 255;
				}
			}
		}
	}
	if ((uint32_t)-1 == scale ||
	    (uint32_t)-1 == slice_num ||
	    (uint32_t)-1 == statistics ||
	    (uint32_t)-1 == test_pulse ||
	    (uint32_t)-1 == mode) {
		log_error(LOGL, NAME": No data.");
		goto pnpi_cros3_rewrite_data_done;
	}
	CHECK_ACCESS(&r16[1]);
	CHECK_BITS("footer 0", r16[0], 0xff00, 0xde00);
	CHECK_BITS("footer 1", r16[1], 0xff00, 0xde00);

	/* Write header + edges. */
	w32 = a_event_buffer->ptr;
	header = w32;
	w32 += 2;
	for (ad16_i = 0; LENGTH(ad16_array) > ad16_i; ++ad16_i) {
		struct AD16 const *ad16;
		size_t ch_i;

		ad16 = &ad16_array[ad16_i];
		for (ch_i = 0; LENGTH(ad16->channel_array) > ch_i; ++ch_i) {
			struct Channel const *channel;
			size_t hit_i;
			uint8_t hn;

			channel = &ad16->channel_array[ch_i];
			hn = hit_num[ad16_i][ch_i];
			for (hit_i = 0; hn > hit_i; ++hit_i) {
				*w32++ = channel->hit_array[hit_i].te << 16 |
				    (ad16_i << 4 | ch_i) << 8 |
				    channel->hit_array[hit_i].le;
			}
		}
	}
	header[0] = trigger_num << 28 | ccb_id << 24 | trigger_time << 20 |
	    0x40000 | (w32 - header - 2);
	header[1] = scale << 20 | slice_num << 12 | statistics << 8 |
	    test_pulse << 4 | mode;
	end = w32;
	ASSERT(unsigned, "u", 0, ==, (unsigned)(3 & (intptr_t)end));
	result = 0;
	EVENT_BUFFER_ADVANCE(*a_event_buffer, end);
pnpi_cros3_rewrite_data_done:
	LOGF(spam)(LOGL, NAME" rewrite_data(0x%08x) }", result);
	return result;
}
