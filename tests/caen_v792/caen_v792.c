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
#include <math.h>
#include <config/parser.h>
#include <module/caen_v792/caen_v792.h>
#include <module/caen_v792/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct CaenV792Module *v792;
	struct Module *module;

	config_load_without_global("tests/caen_v792/empty.cfg");

	block = config_get_block(NULL, KW_CAEN_V792);
	NTRY_PTR(NULL, !=, block);
	v792 = (void *)module_create(NULL, KW_CAEN_V792, block);

	NTRY_I(KW_CAEN_V792, ==, v792->v7nn.module.type);
	NTRY_I(32, ==, v792->v7nn.module.event_max);
	NTRY_PTR(NULL, !=, v792->v7nn.module.pedestal.array);
	NTRY_I(32, ==, v792->v7nn.module.pedestal.array_len);

	module = &v792->v7nn.module;
	module_free(&module);
}

NTEST(IPEDConversion)
{
	float current;
	int i;

	/* Test table values. */
	NTRY_DBL(1e-3, >, fabs( 1.94 - caen_v792_iped_to_current(  0)));
	NTRY_DBL(1e-3, >, fabs( 1.94 - caen_v792_iped_to_current( 60)));
	NTRY_DBL(1e-3, >, fabs( 3.77 - caen_v792_iped_to_current( 65)));
	NTRY_DBL(1e-3, >, fabs( 5.79 - caen_v792_iped_to_current( 70)));
	NTRY_DBL(1e-3, >, fabs( 7.77 - caen_v792_iped_to_current( 75)));
	NTRY_DBL(1e-3, >, fabs( 9.84 - caen_v792_iped_to_current( 80)));
	NTRY_DBL(1e-3, >, fabs(13.93 - caen_v792_iped_to_current( 90)));
	NTRY_DBL(1e-3, >, fabs(18.04 - caen_v792_iped_to_current(100)));
	NTRY_DBL(1e-3, >, fabs(24.26 - caen_v792_iped_to_current(115)));
	NTRY_DBL(1e-3, >, fabs(30.58 - caen_v792_iped_to_current(130)));
	NTRY_DBL(1e-3, >, fabs(36.83 - caen_v792_iped_to_current(145)));
	NTRY_DBL(1e-3, >, fabs(43.07 - caen_v792_iped_to_current(160)));
	NTRY_DBL(1e-3, >, fabs(49.34 - caen_v792_iped_to_current(175)));
	NTRY_DBL(1e-3, >, fabs(55.60 - caen_v792_iped_to_current(190)));
	NTRY_DBL(1e-3, >, fabs(61.83 - caen_v792_iped_to_current(205)));
	NTRY_DBL(1e-3, >, fabs(68.06 - caen_v792_iped_to_current(220)));
	NTRY_DBL(1e-3, >, fabs(74.36 - caen_v792_iped_to_current(235)));
	NTRY_DBL(1e-3, >, fabs(80.58 - caen_v792_iped_to_current(250)));
	NTRY_DBL(1e-3, >, fabs(82.69 - caen_v792_iped_to_current(255)));
	NTRY_DBL(1e-3, >, fabs(82.69 - caen_v792_iped_to_current(300)));

	/* Must be strictly monotonic in table range. */
	current = 0.0f;
	for (i = 60; 256 > i; ++i) {
		float f;

		f = caen_v792_iped_to_current(i);
		NTRY_DBL(current, <, f);
		current = f;
	}
}

NTEST_SUITE(CAEN_V792)
{
	module_setup();

	NTEST_ADD(DefaultConfig);
	NTEST_ADD(IPEDConversion);

	config_shutdown();
}
