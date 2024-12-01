/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2021, 2023-2024
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
#include <module/caen_v965/caen_v965.h>
#include <module/caen_v965/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct ConfigBlock *block;
	struct CaenV965Module *v965;
	struct Module *module;

	config_load_without_global("tests/caen_v965/empty.cfg");

	block = config_get_block(NULL, KW_CAEN_V965);
	NTRY_PTR(NULL, !=, block);
	v965 = (void *)module_create(NULL, KW_CAEN_V965, block);

	NTRY_I(KW_CAEN_V965, ==, v965->v7nn.module.type);
	NTRY_I(32, ==, v965->v7nn.module.event_max);
	NTRY_PTR(NULL, !=, v965->v7nn.module.pedestal.array);
	NTRY_I(32, ==, v965->v7nn.module.pedestal.array_len);

	module = &v965->v7nn.module;
	module_free(&module);
}

NTEST(IPEDConversion)
{
	float current;
	int i;

	NTRY_DBL(1e-3, >, fabs(492.5 - caen_v965_iped_to_current(  0)));
	NTRY_DBL(1e-3, >, fabs(582.5 - caen_v965_iped_to_current(180)));
	NTRY_DBL(1e-3, >, fabs(620.0 - caen_v965_iped_to_current(255)));

	/* Must be strictly monotonic in range. */
	current = 0.0f;
	for (i = 0; 256 > i; ++i) {
		float f;

		f = caen_v965_iped_to_current(i);
		NTRY_DBL(current, <, f);
		current = f;
	}
}

NTEST_SUITE(CAEN_V965)
{
	module_setup();

	NTEST_ADD(DefaultConfig);
	NTEST_ADD(IPEDConversion);

	config_shutdown();
}
