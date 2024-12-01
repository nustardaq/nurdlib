/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024
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
#include <module/sis_3820_scaler/sis_3820_scaler.h>
#include <module/sis_3820_scaler/internal.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct Sis3820ScalerModule *s3820;
	struct Module *module;

	config_load_without_global("tests/sis_3820_scaler/empty.cfg");

	block = config_get_block(NULL, KW_SIS_3820_SCALER);
	NTRY_PTR(NULL, !=, block);
	s3820 = (void *)module_create(NULL, KW_SIS_3820_SCALER, block);

	NTRY_I(KW_SIS_3820_SCALER, ==, s3820->module.type);
	NTRY_I(1024, ==, s3820->module.event_max);
	NTRY_I(0, ==, s3820->address);

	module = &s3820->module;
	module_free(&module);
}

NTEST_SUITE(SIS_3820_SCALER)
{
	module_setup();

	NTEST_ADD(DefaultConfig);

	config_shutdown();
}
