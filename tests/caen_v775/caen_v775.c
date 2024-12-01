/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
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

#include <ntest/ntest.h>
#include <config/parser.h>
#include <nurdlib/config.h>
#include <module/caen_v775/caen_v775.h>
#include <module/caen_v775/internal.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct CaenV775Module *v775;
	struct Module *module;

	config_load_without_global("tests/caen_v775/empty.cfg");

	block = config_get_block(NULL, KW_CAEN_V775);
	NTRY_PTR(NULL, !=, block);
	v775 = (void *)module_create(NULL, KW_CAEN_V775, block);

	NTRY_I(KW_CAEN_V775, ==, v775->v7nn.module.type);
	NTRY_I(32, ==, v775->v7nn.module.event_max);

	module = &v775->v7nn.module;
	module_free(&module);
}

NTEST_SUITE(CAEN_V775)
{
	module_setup();

	NTEST_ADD(DefaultConfig);

	config_shutdown();
}
