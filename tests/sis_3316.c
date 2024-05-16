/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2018, 2020-2021, 2023-2024
 * Bastian Löher
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
#include <module/sis_3316/sis_3316.h>
#include <module/sis_3316/internal.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct Sis3316Module *m;
	struct Module *module;

	config_load_without_global("tests/sis_3316_empty.cfg");

	block = config_get_block(NULL, KW_SIS_3316);
	NTRY_PTR(NULL, !=, block);
	m = (void *)module_create(NULL, KW_SIS_3316, block);

	NTRY_I(1, ==, m->config.is_fpbus_master);
	NTRY_I(0, ==, m->config.use_accumulator6);
	NTRY_I(0, ==, m->config.use_accumulator2);
	NTRY_I(1, ==, m->config.use_maxe);

	module = &m->module;
	module_free(&module);
}

NTEST_SUITE(SIS_3316)
{
	module_setup();

	NTEST_ADD(DefaultConfig);

	config_shutdown();
}
