/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2019-2021, 2023-2024
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

#include <ntest/ntest.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <module/map/map.h>
#include <module/mesytec_vmmr8/internal.h>
#include <module/mesytec_vmmr8/offsets.h>

NTEST(DefaultConfig)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecVmmr8Module *vmmr8;

	crate_setup();
	module_setup();
	map_setup();

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_vmmr8/empty.cfg");
	crate = crate_create();

	vmmr8 = (void *)crate_module_find(crate, KW_MESYTEC_VMMR8, 0);
	vmmr8->mxdc32.do_sleep = 0;

	crate_init(crate);

	NTRY_I(KW_MESYTEC_VMMR8, ==, vmmr8->mxdc32.module.type);
	/* Cannot check event_max until after we know the firmware. */

	crate_free(&crate);
	map_user_clear();
	map_shutdown();
	config_shutdown();
}

NTEST_SUITE(MESYTEC_VMMR8)
{
	NTEST_ADD(DefaultConfig);
}
