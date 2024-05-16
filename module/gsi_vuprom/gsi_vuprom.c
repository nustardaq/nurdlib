/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2024
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

#include <module/gsi_vuprom/gsi_vuprom.h>
#include <sched.h>
#include <module/gsi_vuprom/internal.h>
#include <module/gsi_vuprom/offsets.h>
#include <module/map/map.h>
#include <nurdlib/serialio.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/bits.h>
#include <util/time.h>

#define NAME "Gsi Vuprom"
#define FIRMWARE 0x0a060202
#define READOUT_TIMEOUT 1.0

MODULE_PROTOTYPES(gsi_vuprom);
uint32_t	gsi_vuprom_read_fifo(struct GsiVupromModule *, struct
    EventBuffer *);
uint32_t	gsi_vuprom_read_raw(struct GsiVupromModule *, struct
    EventBuffer *);

uint32_t
gsi_vuprom_check_empty(struct Module *a_module)
{
	struct GsiVupromModule *vuprom;
	uint32_t result;

	LOGF(spam)(LOGL, NAME" check_empty {");
	MODULE_CAST(KW_GSI_VUPROM, vuprom, a_module);
	result = 0 == MAP_READ(vuprom->sicy_map, data_ready) ? 0 :
	    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	LOGF(spam)(LOGL, NAME" check_empty(0x%08x) }", result);
	return result;
}

struct Module *
gsi_vuprom_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiVupromModule *vuprom;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(vuprom);
	vuprom->module.event_max = 1;
	vuprom->address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address=%08x.", vuprom->address);
	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)vuprom;
}

void
gsi_vuprom_deinit(struct Module *a_module)
{
	struct GsiVupromModule *vuprom;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_GSI_VUPROM, vuprom, a_module);
	map_unmap(&vuprom->sicy_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
gsi_vuprom_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
gsi_vuprom_get_map(struct Module *a_module)
{
	struct GsiVupromModule *vuprom;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_GSI_TRIVA, vuprom, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return vuprom->sicy_map;
}

void
gsi_vuprom_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		BITS_MASK(16, 23),
		BITS_MASK(24, 31) | BITS_MASK(9, 15),
		0xfe << 24)
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
gsi_vuprom_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct ModuleGate gate;
	struct GsiVupromModule *vuprom;
	unsigned full_range;
	int do_zero_suppress;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_GSI_VUPROM, vuprom, a_module);

	do_zero_suppress = config_get_boolean(a_module->config,
	    KW_ZERO_SUPPRESS);
	LOGF(verbose)(LOGL, "Zero suppress=%s.", do_zero_suppress ? "yes" :
	    "no");
	MAP_WRITE(vuprom->sicy_map, zero_suppress, do_zero_suppress);

	vuprom->use_fifo = config_get_boolean(a_module->config, KW_USE_FIFO);
	LOGF(verbose)(LOGL, "Use fifo=%s.", vuprom->use_fifo ? "yes" : "no");

	/* Set TDC range in 2.5 ns steps (valid only every 20 ns). */
	/* TODO: Check that it rounds to be more conservative. */
	module_gate_get(&gate, a_module->config, 0, 0, 0.0, 2500);
	full_range = gate.width_ns / 2.5;
	LOGF(verbose)(LOGL, "Full range=0x%08x, %g ns.", full_range,
	    gate.width_ns);
	MAP_WRITE(vuprom->sicy_map, full_range, full_range);

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
gsi_vuprom_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiVupromModule *vuprom;
	size_t i;
	uint32_t firmware;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_GSI_VUPROM, vuprom, a_module);

	/* Mapping. */
	vuprom->sicy_map = map_map(vuprom->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(full_range), MAP_POKE_REG(full_range), 0);

	/* Firmware info. */
	firmware = MAP_READ(vuprom->sicy_map, firmware_date);
	LOGF(verbose)(LOGL, "Firmware: 20%02d/%02d/%02d v%02d, 0x%08x",
		(firmware >> 8) & 0xff, (firmware >> 16) & 0xff,
		(firmware >> 24) & 0xff, firmware & 0xff, firmware);

	/* Initialise clock. */
	while (0x0003 != MAP_READ(vuprom->sicy_map, clock_status)) {
		LOGF(spam)(LOGL, "setting clock and waiting 0.01 s.");
		MAP_WRITE(vuprom->sicy_map, clock_status, 0x0003);
		time_sleep(10e-3);
	}

	/* Enable all channels. */
	for (i = 0; i < 6; ++i)	{
		MAP_WRITE(vuprom->sicy_map, disable(i), 0x0);
	}

	/* Standard setup. */
	MAP_WRITE(vuprom->sicy_map, trig_sel, 3);
	MAP_WRITE(vuprom->sicy_map, data_ready, 1);
	MAP_WRITE(vuprom->sicy_map, trig_out, 0xffffffff);
	MAP_WRITE(vuprom->sicy_map, scaler_clr, 1);
	MAP_WRITE(vuprom->sicy_map, vme_test, 0);

	/* Copy settings from config. */
	MAP_WRITE(vuprom->sicy_map, module_id, vuprom->module.id);

	/* Trigger mode (what comes out on the outputs) */
	MAP_WRITE(vuprom->sicy_map, trig_sel, 0x120);

	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
gsi_vuprom_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_vuprom_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct GsiVupromModule *vuprom;
	uint32_t const *p;
	uint32_t const *end;
	uint32_t result;
	uint32_t id, count;

	(void)a_crate;
	(void)a_do_pedestals;

	LOGF(spam)(LOGL, NAME" parse_data {");
	MODULE_CAST(KW_GSI_VUPROM, vuprom, a_module);

	result = 0;
	if (0 != a_event_buffer->bytes % sizeof(uint32_t)) {
		log_error(LOGL, "Data_size not 32-bit aligned.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto gsi_vuprom_parse_data_end;
	}

	p = a_event_buffer->ptr;
	end = p + a_event_buffer->bytes / sizeof(uint32_t);
	if (0xfe000000 != (0xff000000 & *p)) {
		log_error(LOGL, "Header corrupt.");
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto gsi_vuprom_parse_data_end;
	}
	id = (0x00ff0000 & *p) >> 16;
	if (id != vuprom->module.id) {
		log_die(LOGL, "Id mismatch, expected 0x%02x,  got 0x%02x!",
		    vuprom->module.id, id);
		result = CRATE_READOUT_FAIL_DATA_CORRUPT;
		goto gsi_vuprom_parse_data_end;
	}
	count = 0x000001ff & *p;
	if ((unsigned)(end - p) != count + 1) {
		log_die(LOGL, "Missing data, expect %d (header 0x%08x), got "
		    "%d.", count, *p, (int)(end - p));
		result = CRATE_READOUT_FAIL_DATA_MISSING;
		goto gsi_vuprom_parse_data_end;
	}
	++p;
	++vuprom->counter;

gsi_vuprom_parse_data_end:
	LOGF(spam)(LOGL, NAME" parse_data }");
	return result;
}

uint32_t
gsi_vuprom_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiVupromModule *vuprom;
	double t_0;
	uint32_t result;

	(void)a_crate;

	result = 0;

	LOGF(spam)(LOGL, NAME" readout {");
	MODULE_CAST(KW_GSI_VUPROM, vuprom, a_module);

	for (t_0 = -1;;) {
		double t;

		if (0 != (0x8 & MAP_READ(vuprom->sicy_map, data_ready))) {
			break;
		}
		t = time_getd();
		if (0 > t_0) {
			t_0 = t + READOUT_TIMEOUT;
		} else if (t > t_0) {
			log_error(LOGL, NAME" data timeout (>%fs).",
			    READOUT_TIMEOUT);
			result = CRATE_READOUT_FAIL_DATA_MISSING;
			goto gsi_vuprom_readout_done;
		}
		sched_yield();
		crate_acvt_grow(a_crate);
	}

	if (vuprom->has_data) {
		if (vuprom->use_fifo) {
			result = gsi_vuprom_read_fifo(vuprom, a_event_buffer);
		} else {
			result = gsi_vuprom_read_raw(vuprom, a_event_buffer);
		}
	}
gsi_vuprom_readout_done:
	MAP_WRITE(vuprom->sicy_map, data_ready, 1);
	LOGF(spam)(LOGL, NAME" readout }");
	return result;
}

/*
 * The FIFO seems to work only on VUPROM2 modules.
 */
uint32_t
gsi_vuprom_read_fifo(struct GsiVupromModule *vuprom, struct EventBuffer
    *a_event_buffer)
{
	size_t i = 0;
	uint32_t header = 0;
	uint32_t result = 0, count = 0;
	uint32_t *outp = NULL;

	/*
	 * Header content:
	 *   0_8: Number of data words in the fifo
	 *  9_15: 0
	 * 16_23: module id
	 * 24_31: 0xfe
	 */
	header = MAP_READ(vuprom->sicy_map, fifo_size);
	count = 0x1ff & header;
	if (0 == count) {
		LOGF(spam)(LOGL, "Fifo empty.");
		return 0;
	}

	LOGF(spam)(LOGL, "Fifo size=0x%08x, header=0x%08x.", count, header);

	outp = a_event_buffer->ptr;
	/* NOTE: 1 + count words. */
	if (!MEMORY_CHECK(*a_event_buffer, &outp[count])) {
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		return result;
	}
	*outp++ = header;

	for (i = 0; i < count; ++i) {
		*outp++ = MAP_READ(vuprom->sicy_map, fifo_data);
	}
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);

	return result;
}

/*
 * Use the RAW readout for VUPROM3 modules. The data structure is the same
 */
#define N_TRIES 2 /* was 3 in apr2012 daq */
#define N_CH 128 /* 128 was in apr2012 daq (why not 192 ?) */
uint32_t
gsi_vuprom_read_raw(struct GsiVupromModule *vuprom, struct EventBuffer
    *a_event_buffer)
{
	unsigned i = 0;
	unsigned n = 0;
	uint32_t *header = NULL;
	uint32_t *outp = NULL;
	uint32_t count = 0;

	uint32_t data[N_TRIES][N_CH];

	header = a_event_buffer->ptr;
	outp = header + 1;

	/* Read data. */
	for (n = 0; n < N_TRIES; ++n) {
		for (i = 0; i < N_CH; ++i) {
			data[n][i] = MAP_READ(vuprom->sicy_map, tdc_data(i));
		}
		SERIALIZE_IO;
	}

	/* Write data. */
	for (i = 0; i < N_CH; ++i) {
		for (n = 0; n < N_TRIES; ++n) {
			if (data[n][i] & 0x0000ffff) {
				if (!MEMORY_CHECK(*a_event_buffer, outp)) {
					log_error(LOGL, "Destination buffer "
					    "too small.");
					return
					    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
				}
				*outp++ = data[n][i];
				break;
			}
		}
	}

	/* Write header. */
	count = (outp - header) - 1;
	*header = 0xfeUL << 24		/* Header marker. */
	    | (uint32_t) ((vuprom->module.id & 0xff) << 16)
	    | (0x01 << 9)		/* RAW marker. */
	    | (count & 0x1ff);

	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);

	return 0;
}

uint32_t
gsi_vuprom_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiVupromModule *vuprom;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_GSI_VUPROM, vuprom, a_module);
	a_module->event_counter.value = vuprom->counter;
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x) }",
	    a_module->event_counter.value);
	return 0;
}

void
gsi_vuprom_setup_(void)
{
	MODULE_SETUP(gsi_vuprom, 0);
}
