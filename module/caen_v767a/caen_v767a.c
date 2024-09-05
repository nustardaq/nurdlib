/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024
 * Günter Weber
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

#include <module/caen_v767a/caen_v767a.h>
#include <module/caen_v767a/internal.h>
#include <module/caen_v767a/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/bits.h>
#include <util/math.h>
#include <util/time.h>
#include <util/string.h>
#include <unistd.h>
#include <stdarg.h>

#define NAME "CEAN_V767A"

MODULE_PROTOTYPES(caen_v767a);

static void write_op(struct Map *, uint16_t, int, ...);
static void read_op(struct Map *, int16_t *);
static int await_handshake(struct Map *, uint16_t);

uint32_t
caen_v767a_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
caen_v767a_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct CaenV767AModule *caen_v767a;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(caen_v767a);
	/* 10 bit event counter, see p. 32 of manual */
	caen_v767a->module.event_counter.mask = 0x3FF;
	caen_v767a->module.event_max = 32;
	caen_v767a->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", caen_v767a->address);
	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)caen_v767a;
}

void
caen_v767a_deinit(struct Module *a_module)
{
	struct CaenV767AModule *caen_v767a;

	LOGF(info)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_CAEN_V767A, caen_v767a, a_module);
	map_unmap(&caen_v767a->sicy_map);
	LOGF(info)(LOGL, NAME" deinit }");
}

void
caen_v767a_destroy(struct Module *a_module)
{
	(void)a_module;
}

struct Map *
caen_v767a_get_map(struct Module *a_module)
{
	struct CaenV767AModule *caen_v767a;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_CAEN_V767A, caen_v767a, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return caen_v767a->sicy_map;
}

void
caen_v767a_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	LOGF(verbose)(LOGL, NAME" get_signature {");
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(0, 31),
		0x767a767a)
	MODULE_SIGNATURE_END(a_array, a_num)
	LOGF(verbose)(LOGL, NAME" get_signature {");
}

void
write_op(struct Map *map, uint16_t data, int arg_num, ...)
{
	int i_arg = 0;
	va_list arg_list;

	LOGF(spam)(LOGL, NAME" write_op(data=0x%04x,argn=%d) {",
	    data, arg_num);
	if ( await_handshake(map, 0x02) != 0 ) {
		log_die(LOGL, "Waiting for handshake timed out.");
	}
	time_sleep(10e-3);
	va_start(arg_list, arg_num);
	MAP_WRITE(map, opcode, data);
	for ( i_arg = 0; i_arg < arg_num; i_arg++ ) {
		int arg;

		arg = va_arg(arg_list, int);
		LOGF(spam)(LOGL, "%d: 0x%04x.", i_arg, arg);
		MAP_WRITE(map, opcode, arg);
	}
	va_end(arg_list);
	LOGF(spam)(LOGL, NAME" write_op }");
}

void
read_op(struct Map *map, int16_t *data)
{
	LOGF(spam)(LOGL, NAME" read_op {");
	if ( await_handshake(map, 0x01) != 0 ) {
		log_die(LOGL, "Waiting for handshake timed out.");
	}
	time_sleep(10e-3);
	*data = MAP_READ(map, opcode);
	LOGF(spam)(LOGL, NAME" read_op(data=0x%04x) }", *data);
}

int
await_handshake(struct Map *map, uint16_t flags)
{
	uint16_t rdata = 0;
	int loop_time = 0;
	int ret = -1;

	LOGF(spam)(LOGL, NAME" await_handshake(flags=0x%04x) {", flags);
	rdata = MAP_READ(map, opcode_handshake);
	while ( (rdata & flags) != flags ) {
		time_sleep(1e-3);
		rdata = MAP_READ(map, opcode_handshake);
		loop_time++;
		if ( loop_time > 2000 ) {
			goto done;
		}
	}
	ret = 0;
done:
	LOGF(spam)(LOGL, NAME" await_handshake }");
	return ret;
}

int
caen_v767a_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV767AModule *caen_v767a;
	int16_t window_setting;
	int16_t buffer = 0;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_CAEN_V767A, caen_v767a, a_module);

	/* Set trigger mode to stop matched. */
	write_op(caen_v767a->sicy_map, 0x1000, 0);

	window_setting = config_get_int32(caen_v767a->module.config, KW_WIDTH,
	    CONFIG_UNIT_NS, 0x0001 * 25, 0x84D0 * 25) / 25;
	/* Set window length. */
	write_op(caen_v767a->sicy_map, 0x3000, 1, window_setting);
	write_op(caen_v767a->sicy_map, 0x3100, 0);
	read_op(caen_v767a->sicy_map, &buffer);
	if ( window_setting != buffer ) {
		log_die(LOGL, NAME" Device did not set window length.");
	}

	window_setting = config_get_int32(caen_v767a->module.config,
	    KW_OFFSET, CONFIG_UNIT_NS,
	    -319999 * 25,
	    (window_setting + 2000 - 1) * 25) / 25;
	/* Set window offset. */
	write_op(caen_v767a->sicy_map, 0x3200, 1, window_setting);
	write_op(caen_v767a->sicy_map, 0x3300, 0);
	read_op(caen_v767a->sicy_map, &buffer);
	if ( window_setting != buffer ) {
		log_die(LOGL, NAME" Device did not set window offset.");
	}

	/* Set data ready status flag. */
	write_op(caen_v767a->sicy_map, 0x7000, 0);

	/*
	 * Need to wait for last op to finished, as the men in black
	 * manipulate the event count in the background.
	 */
	time_sleep(1e-3);

	/* Will not set event counter to zero. */
	MAP_WRITE(caen_v767a->sicy_map, clear, 0xFFFF);
	/* Setting event counter to zero. */
	MAP_WRITE(caen_v767a->sicy_map, clear_event_counter, 0xFFFF);
	while (!(MAP_READ(caen_v767a->sicy_map, status_alt) & 0x1)) {
		(void)(!MAP_READ(caen_v767a->sicy_map, output_buffer));
	}

	LOGF(info)(LOGL, NAME" init_fast }");

	return 1;
}

int
caen_v767a_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV767AModule *caen_v767a;
	int32_t buffer;
	(void)a_crate;

	LOGF(info)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_CAEN_V767A, caen_v767a, a_module);

	caen_v767a->sicy_map = map_map(caen_v767a->address, MAP_SIZE,
	    KW_NOBLT, 0, 0,
	    MAP_POKE_REG(geo_address), MAP_POKE_REG(testword_h), 0);

	READ_N_BYTES_INTO(caen_v767a->sicy_map, manufacturer_id, 3, buffer);
	if ( 0x0040e6 != buffer ) {
		log_die(LOGL,
		    "Manufacturer ID is 0x%06x != 0x0040e6, module borked?",
		    buffer);
	}
	LOGF(info)(LOGL, NAME" Manufacturer ID = %06x.",buffer);

	READ_N_BYTES_INTO(caen_v767a->sicy_map, board_id, 4, buffer);
	if ( 0x000002ff != buffer ) {
		log_die(LOGL,
		    "Board ID is 0x%08x != 0x000002ff, module borked?",
		    buffer);
	}
	LOGF(info)(LOGL, NAME" Board ID = %08x",buffer);

	LOGF(info)(LOGL, NAME" Board revision = %02x",
		READ_BYTE_FROM_WORDS(caen_v767a->sicy_map, revision, 0)
	);

	READ_N_BYTES_INTO(caen_v767a->sicy_map, serial_number, 2, buffer);
	LOGF(info)(LOGL, NAME" Serial number = %04x",buffer);

	LOGF(verbose)(LOGL, NAME" Waiting for module reset...");
	/* Reset module. */
	MAP_WRITE(caen_v767a->sicy_map, single_shot_reset, 0x0);
	/* TODO: Timeout not important here? */
	await_handshake(caen_v767a->sicy_map, 0x02);

	LOGF(info)(LOGL, NAME" init_slow }");

	return 1;
}

void
caen_v767a_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
caen_v767a_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	(void)a_module;
	(void)a_crate;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	return 0;
}

uint32_t
caen_v767a_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct CaenV767AModule *caen_v767a;
	uint32_t *outp, result;
	int event_num;
	int is_eob;
	uint32_t event_counter;
	uint32_t *hit_counter;

	uint16_t status;
	uint32_t datum;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");
	result = 0;

	MODULE_CAST(KW_CAEN_V767A, caen_v767a, a_module);
	outp = a_event_buffer->ptr;

	do {
		status = MAP_READ(caen_v767a->sicy_map, status);
	} while (!(status & 0x01));

	/* Write barrier. */
	*outp++ = 0x767a767a;
	*outp++ = caen_v767a->address;

	datum = MAP_READ(caen_v767a->sicy_map, output_buffer);
	if (((datum >> 21) & 0x03) != 0x02) {
		log_die(LOGL, "Header expected.");
	}
	event_num = datum & 0xFFF;
	LOGF(spam)(LOGL, NAME" EVENT Number: %d", event_num);

	is_eob = 0;
	event_counter = 0;
	hit_counter = outp++;
	do {
		datum = MAP_READ(caen_v767a->sicy_map, output_buffer);
		switch ((datum >> 21) & 0x03) {
		case 0x0:
			{
				LOGF(spam)(LOGL, NAME" DATUM contains: "
				    "time=%d, channel=%02x",
				    datum & 0xFFFFF,
				    (datum >> 24) & 0x7F);
				if ( (uintptr_t)outp >=
				    (uintptr_t)(a_event_buffer->ptr) +
				    a_event_buffer->bytes - sizeof datum ) {
					log_die(LOGL, "Event buffer too "
					    "small to fit all events.");
				}
				*outp++ = datum;
				event_counter++;
				break;
			}
		case 0x1:
			{
				if ((datum & 0xFFFF) != event_counter) {
					log_error(LOGL, "Mismatching datum "
					    "counters in block (exp: %d, "
					    "act: %d).",
					    datum & 0xFFFF,
					    event_counter);
				}
				*hit_counter = event_counter;
				LOGF(spam)(LOGL, NAME" HIT Counter: %d",
				    event_counter);
				is_eob = 1;
				break;
			}
		default:
			log_error(LOGL,
			    "Unexpected or invalid datum in block.");
		}
	} while (is_eob != 1);

	if (!(MAP_READ(caen_v767a->sicy_map, status_alt) & 0x1)) {
		log_die(LOGL,
		    "Event buffer on device is not empty after readout.");
	}

	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout }");
	return result;
}

uint32_t
caen_v767a_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct CaenV767AModule *caen_v767a;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_CAEN_V767A, caen_v767a, a_module);
	caen_v767a->module.event_counter.value =
	    MAP_READ(caen_v767a->sicy_map, event_counter);
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=%08x) }",
	    caen_v767a->module.event_counter.value);
	return 0;
}

void
caen_v767a_setup_(void)
{
	MODULE_SETUP(caen_v767a, 0);
}
