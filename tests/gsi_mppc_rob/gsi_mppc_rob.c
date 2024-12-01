/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2022, 2024
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
#include <module/gsi_pex/internal.h>
#if NCONF_mGSI_PEX_bYES
#	include <module/gsi_mppc_rob/gsi_mppc_rob.h>
#	include <module/gsi_mppc_rob/internal.h>
#	include <config/parser.h>
#	include <module/module.h>
#	include <nurdlib/config.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct GsiMppcRobModule *mppc_rob;
	struct Module *module;

	config_load_without_global("tests/gsi_mppc_rob/simple.cfg");

	block = config_get_block(NULL, KW_GSI_MPPC_ROB);
	NTRY_PTR(NULL, !=, block);
	mppc_rob = (void *)module_create(NULL, KW_GSI_MPPC_ROB, block);
	config_touched_assert(NULL, 0);

	NTRY_I(KW_GSI_MPPC_ROB, ==, mppc_rob->ctdcp.module.type);
	NTRY_I(1, ==, mppc_rob->ctdcp.module.event_max);

	NTRY_I(2, ==, mppc_rob->ctdcp.sfp_i);
	NTRY_I(3, ==, mppc_rob->ctdcp.card_num);

	module = &mppc_rob->ctdcp.module;
	module_free(&module);
}

NTEST_SUITE(GSI_MPPC_ROB)
{
	module_setup();

	NTEST_ADD(DefaultConfig);

	config_shutdown();
}

#endif
