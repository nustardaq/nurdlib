/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
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

#include <module/map/internal.h>

#ifdef POKE_SIGBUS

#	include <nurdlib/base.h>
#	include <nurdlib/log.h>
#	include <util/sigbus.h>

#	define POKE_BEGIN(type_) do {\
		struct Map *mapper;\
		sigbus_set_ok(1);\
		CALLOC(mapper, 1);\
		mapper->type = MAP_TYPE_SICY;\
		mapper->address = a_address + a_ofs_##type_;\
		mapper->bytes = 4;\
		sicy_map(mapper);\
		LOGF(verbose)(LOGL, "Poke_"#type_" %u...", a_bits_##type_)
#	define POKE_END(type_) \
		if (sigbus_was()) {\
			log_die(LOGL, "Poke_"#type_"=0x%08x+0x%"PRIpx"/%u "\
			    "failed, wrong address?", a_address,\
			    a_ofs_##type_, a_bits_##type_);\
		}\
		map_unmap(&mapper);\
		sigbus_set_ok(0);\
	} while (0)

void
poke_r(uint32_t a_address, uintptr_t a_ofs_r, unsigned a_bits_r)
{
	uint32_t value;

	(void)value;
	POKE_BEGIN(r);
	value = map_sicy_read(mapper, MAP_MOD_R, a_bits_r, 0);
	POKE_END(r);
}

void
poke_w(uint32_t a_address, uintptr_t a_ofs_w, unsigned a_bits_w, uint32_t
    a_value_w)
{
	POKE_BEGIN(w);
	map_sicy_write(mapper, MAP_MOD_W, a_bits_w, 0, a_value_w);
	POKE_END(w);
}

#endif
