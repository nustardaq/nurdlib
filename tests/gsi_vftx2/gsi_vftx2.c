/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
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
#include <module/gsi_vftx2/gsi_vftx2.h>
#include <module/gsi_vftx2/internal.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct GsiVftx2Module *vftx2;
	struct Module *module;

	config_load_without_global("tests/gsi_vftx2/empty.cfg");

	block = config_get_block(NULL, KW_GSI_VFTX2);
	NTRY_PTR(NULL, !=, block);
	vftx2 = (void *)module_create(NULL, KW_GSI_VFTX2, block);

	NTRY_I(KW_GSI_VFTX2, ==, vftx2->module.type);
	NTRY_I(1, ==, vftx2->module.event_max);

	NTRY_U(32, ==, vftx2->channel_num);

	module = &vftx2->module;
	module_free(&module);
}

NTEST_SUITE(GSI_VFTX2)
{
	module_setup();

	NTEST_ADD(DefaultConfig);

	config_shutdown();
}
