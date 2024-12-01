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
#include <module/caen_v1290/caen_v1290.h>
#include <module/caen_v1290/internal.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct CaenV1290Module *v1290;
	struct Module *module;

	config_load_without_global("tests/caen_v1290/empty.cfg");

	block = config_get_block(NULL, KW_CAEN_V1290);
	NTRY_PTR(NULL, !=, block);
	v1290 = (void *)module_create(NULL, KW_CAEN_V1290, block);

	NTRY_I(KW_CAEN_V1290, ==, v1290->v1n90.module.type);
	NTRY_I(1024, ==, v1290->v1n90.module.event_max);

	module = &v1290->v1n90.module;
	module_free(&module);
}

NTEST(EdgeResolutions)
{
	struct CaenV1290Module *v1290[4];
	struct ConfigBlock *block;
	size_t i;

	config_load_without_global("tests/caen_v1290/edge.cfg");

	block = config_get_block(NULL, KW_CAEN_V1290);
	for (i = 0; 4 > i; ++i) {
		NTRY_PTR(NULL, !=, block);
		v1290[i] = (void *)module_create(NULL, KW_CAEN_V1290, block);
		block = config_get_block_next(block, KW_CAEN_V1290);
	}

	for (i = 0; 4 > i; ++i) {
		struct Module *module;

		module = &v1290[i]->v1n90.module;
		module_free(&module);
	}
}

/* See v1190.c. */
/*
NTEST(Gates)
{
	uint16_t const c_ofs[] = {40, 0xf800};
	uint16_t const c_width[] = {1, 4095};
	struct ConfigBlock *block;
	size_t i;

	config_load_without_global("tests/caen_v1290_gate.cfg");

	block = config_get_block(NULL, KW_CAEN_V1290);
	for (i = 0; 2 > i; ++i) {
		struct Module *module;
		struct CaenV1290Module *v1290;

		NTRY_PTR(NULL, !=, block);
		module = module_create(NULL, KW_CAEN_V1290, block);
		v1290 = (void *)module;
		NTRY_I(c_ofs[i], ==, v1290->v1n90.gate_offset);
		NTRY_I(c_width[i], ==, v1290->v1n90.gate_width);
		module_free(&module);
		block = config_get_block_next(block, KW_CAEN_V1290);
	}
}
*/

NTEST(Parse)
{
	uint32_t const c_buf[] = {
		0x1f901f90, /* DMA filler. */
		0x40000000, /* Header. */
		0x08000000, /* TDC header. */
		0x00000000, /* TDC measurement. */
		0x20000000, /* Error bits. */
		0x18000000, /* TDC trailer. */
		0x88000000, /* Extented time tag. */
		0x80000000, /* Trailer. */
	};
	struct EventConstBuffer eb;
	struct ConfigBlock *block;
	struct CaenV1290Module *v1290;
	struct Module *module;
	int ret;

	config_load_without_global("tests/caen_v1290/empty.cfg");

	block = config_get_block(NULL, KW_CAEN_V1290);
	NTRY_PTR(NULL, !=, block);
	v1290 = (void *)module_create(NULL, KW_CAEN_V1290, block);

	eb.bytes = sizeof c_buf;
	eb.ptr = c_buf;
	ret = v1290->v1n90.module.props->parse_data(NULL,
	    &v1290->v1n90.module, &eb, 0);
	NTRY_I(0, ==, ret);

	module = &v1290->v1n90.module;
	module_free(&module);
}

NTEST_SUITE(CAEN_V1290)
{
	module_setup();

	NTEST_ADD(DefaultConfig);
	NTEST_ADD(EdgeResolutions);
/*	NTEST_ADD(Gates); */
	NTEST_ADD(Parse);

	config_shutdown();
}
