/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015, 2017-2021, 2024
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
#include <module/gsi_tridi/gsi_tridi.h>
#include <module/trloii/internal.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct TRLOIIModule *tridi;
	struct Module *module;

	config_load_without_global("tests/gsi_tridi/empty.cfg");

	block = config_get_block(NULL, KW_GSI_TRIDI);
	NTRY_PTR(NULL, !=, block);
	tridi = (void *)module_create(NULL, KW_GSI_TRIDI, block);

	NTRY_I(KW_GSI_TRIDI, ==, tridi->module.type);
	NTRY_I(170, ==, tridi->module.event_max);

	module = &tridi->module;
	module_free(&module);
}

NTEST_SUITE(GSI_TRIDI)
{
	module_setup();

	NTEST_ADD(DefaultConfig);

	config_shutdown();
}
