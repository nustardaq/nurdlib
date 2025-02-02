/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2024-2025
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
#include <module/gsi_sam/gsi_sam.h>
#include <module/gsi_sam/internal.h>
#include <module/gsi_tacquila/internal.h>
#include <module/pnpi_cros3/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct GsiSamModule *sam;
	struct Module *module;

	config_load_without_global("tests/gsi_sam/empty.cfg");

	block = config_get_block(NULL, KW_GSI_SAM);
	NTRY_PTR(NULL, !=, block);
	sam = (void *)module_create(NULL, KW_GSI_SAM, block);

	NTRY_I(KW_GSI_SAM, ==, sam->module.type);
	NTRY_I(1, ==, sam->module.event_max);

	NTRY_PTR(NULL, ==, sam->gtb_client[0]);
	NTRY_PTR(NULL, ==, sam->gtb_client[1]);

	module = &sam->module;
	module_free(&module);
}

NTEST(Tacquila)
{
#if HAS_GSI_TACQUILA
	struct ConfigBlock *block;
	struct GsiSamModule *sam;
	struct Module *module;

	config_load_without_global("tests/gsi_sam/tacquila.cfg");

	block = config_get_block(NULL, KW_GSI_SAM);
	NTRY_PTR(NULL, !=, block);
	sam = (void *)module_create(NULL, KW_GSI_SAM, block);

	NTRY_I(KW_GSI_SAM, ==, sam->module.type);
	NTRY_I(1, ==, sam->module.event_max);

	NTRY_PTR(NULL, ==, sam->gtb_client[0]);
	NTRY_PTR(NULL, !=, sam->gtb_client[1]);
	NTRY_U(KW_GSI_TACQUILA, ==, sam->gtb_client[1]->type);

	module = &sam->module;
	module_free(&module);
#endif
}

NTEST(Cros3)
{
#if HAS_PNPI_CROS3
	struct ConfigBlock *block;
	struct GsiSamModule *sam;
	struct Module *module;

	config_load_without_global("tests/gsi_sam/cros3.cfg");

	block = config_get_block(NULL, KW_GSI_SAM);
	NTRY_PTR(NULL, !=, block);
	sam = (void *)module_create(NULL, KW_GSI_SAM, block);

	NTRY_I(KW_GSI_SAM, ==, sam->module.type);
	NTRY_I(1, ==, sam->module.event_max);

	NTRY_PTR(NULL, ==, sam->gtb_client[0]);
	NTRY_PTR(NULL, !=, sam->gtb_client[1]);
	NTRY_U(KW_PNPI_CROS3, ==, sam->gtb_client[1]->type);

	module = &sam->module;
	module_free(&module);
#endif
}

NTEST(Siderem)
{
#if HAS_GSI_SIDEREM
	struct ConfigBlock *block;
	struct GsiSamModule *sam;
	struct Module *module;

	config_load_without_global("tests/gsi_sam/siderem.cfg");

	block = config_get_block(NULL, KW_GSI_SAM);
	NTRY_PTR(NULL, !=, block);
	sam = (void *)module_create(NULL, KW_GSI_SAM, block);

	NTRY_I(KW_GSI_SAM, ==, sam->module.type);
	NTRY_I(1, ==, sam->module.event_max);

	NTRY_PTR(NULL, ==, sam->gtb_client[0]);
	NTRY_PTR(NULL, !=, sam->gtb_client[1]);
	NTRY_U(KW_GSI_SIDEREM, ==, sam->gtb_client[1]->type);

	module = &sam->module;
	module_free(&module);
#endif
}

NTEST(Mixed)
{
#if HAS_GSI_TACQUILA
	struct ConfigBlock *block;
	struct GsiSamModule *sam;

	config_load_without_global("tests/gsi_sam/mixed.cfg");
	block = config_get_block(NULL, KW_GSI_SAM);
	NTRY_PTR(NULL, !=, block);
	NTRY_SIGNAL(sam = (void *)module_create(NULL, KW_GSI_SAM, block));
	(void)sam;
#endif
}

NTEST(OverloadForbidden)
{
#if HAS_GSI_TACQUILA || HAS_PNPI_CROS3
	struct ConfigBlock *block;
	struct GsiSamModule *sam;

	config_load_without_global("tests/gsi_sam/overload.cfg");
	block = config_get_block(NULL, KW_GSI_SAM);
	NTRY_PTR(NULL, !=, block);
	NTRY_SIGNAL(sam = (void *)module_create(NULL, KW_GSI_SAM, block));
	(void)sam;
#endif
}

NTEST_SUITE(GSI_SAM)
{
	module_setup();

	NTEST_ADD(DefaultConfig);
	NTEST_ADD(Tacquila);
	NTEST_ADD(Cros3);
	NTEST_ADD(Siderem);
	NTEST_ADD(Mixed);
	NTEST_ADD(OverloadForbidden);

	config_shutdown();
}
