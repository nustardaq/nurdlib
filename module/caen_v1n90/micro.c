/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019, 2021, 2023-2024
 * Stephane Pietri
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

#include <module/caen_v1n90/micro.h>
#include <unistd.h>
#include <module/caen_v1n90/internal.h>
#include <module/caen_v1n90/offsets.h>
#include <module/map/map.h>
#include <nurdlib/log.h>
#include <util/assert.h>
#include <util/time.h>

/* With ~10ms sleep = 1s total wait for handshake. */
#define MICRO_TRIES 100

static void	write_common(struct ModuleList const *, int, uint16_t,
    uintptr_t, int *);

uint16_t
micro_read(struct CaenV1n90Module const *a_module, int *a_skip)
{
	int tries;

	LOGF(verbose)(LOGL, "micro_read:%u: skip=%d", a_module->module.id, *a_skip);
	if (*a_skip) {
		return 0;
	}
	for (tries = 0; MICRO_TRIES > tries; ++tries) {
		if (0 != (0x2 &
		    MAP_READ(a_module->sicy_map, micro_handshake))) {
			uint16_t u16;

			u16 = MAP_READ(a_module->sicy_map, micro);
			LOGF(verbose)(LOGL, "micro_read=0x%04x", u16);
			return u16;
		}
		time_sleep(10e-3);
	}
	log_error(LOGL, "Handshake timeout on micro read.");
	*a_skip = 1;
	return 0;
}

void
micro_write(struct ModuleList const *a_list, uint16_t a_opcode, int *a_skip)
{
	write_common(a_list, 0, a_opcode, 0, a_skip);
}

void
micro_write_member(struct ModuleList const *a_list, uintptr_t a_offset, int
    *a_skip)
{
	write_common(a_list, 1, 0, a_offset, a_skip);
}

/*
 * This writes a constant or the value of a struct member with the offset in
 * 'a_value' in parallel to all modules, which reduces all the looping into
 * effectively one module's worth of loops.
 */
void
write_common(struct ModuleList const *a_list, int a_has_offset, uint16_t
    a_value, uintptr_t a_offset, int *a_skip)
{
	/* TODO This only allows modules with ID up to 32... */
	uint32_t is_done_mask;
	int tries;

	LOGF(verbose)(LOGL, "micro_write: has_ofs=%d value=0x%04x ofs=%"PRIp
	    " skip=%d", a_has_offset, a_value, a_offset, *a_skip);
	if (*a_skip) {
		return;
	}
	is_done_mask = 0;
	for (tries = 0; MICRO_TRIES > tries; ++tries) {
		struct Module *module;
		unsigned busy_num;

		busy_num = 0;
		TAILQ_FOREACH(module, a_list, next) {
			struct CaenV1n90Module *v1n90;
			int bit;

			if (KW_CAEN_V1190 != module->type &&
			    KW_CAEN_V1290 != module->type) {
				continue;
			}
			ASSERT(int, "d", 32, >, module->id);
			bit = 1 << module->id;
			if (0 != (bit & is_done_mask)) {
				continue;
			}			++busy_num;
			++busy_num;
			v1n90 = (struct CaenV1n90Module *)module;
			if (0 != (0x1 &
			    MAP_READ(v1n90->sicy_map, micro_handshake))) {
				uint16_t value = a_has_offset ?
				    *((uint16_t *)((uintptr_t)v1n90 +
					a_offset)) : a_value;
				LOGF(verbose)(LOGL, "write=0x%04x.", value);
				MAP_WRITE(v1n90->sicy_map, micro, value);
				is_done_mask |= bit;
			}
		}
		if (0 == busy_num) {
			return;
		}
		time_sleep(10e-3);
	}
	log_error(LOGL, "Handshake timeout on micro write (%s=0x%04x,%p).",
	    a_has_offset ? "offset" : "opcode", a_value, (void *)a_offset);
	*a_skip = 1;
}
