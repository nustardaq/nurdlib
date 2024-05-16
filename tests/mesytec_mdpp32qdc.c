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
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <module/map/map.h>
#include <module/mesytec_mdpp32qdc/internal.h>
#include <module/mesytec_mdpp32qdc/offsets.h>

NTEST(DefaultConfig)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecMdpp32qdcModule *mdpp32qdc;
	unsigned i;

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_mdpp32qdc_empty.cfg");
	crate = crate_create();

	mdpp32qdc = (void *)crate_module_find(crate, KW_MESYTEC_MDPP32QDC, 0);
	mdpp32qdc->mdpp.mxdc32.do_sleep = 0;

	crate_init(crate);

	NTRY_I(KW_MESYTEC_MDPP32QDC, ==, mdpp32qdc->mdpp.mxdc32.module.type);
	NTRY_I(190, ==, mdpp32qdc->mdpp.mxdc32.module.event_max);

	NTRY_I(0, ==, mdpp32qdc->config.adc_resolution);

	NTRY_I(0, ==, mdpp32qdc->mdpp.config.monitor_on);
	NTRY_I(0, ==, mdpp32qdc->mdpp.config.monitor_channel);
	NTRY_I(0, ==, mdpp32qdc->mdpp.config.monitor_wave);

	i = 0;
	NTRY_I(KW_BUSY, ==, mdpp32qdc->mdpp.config.nim[i++]); /* Busy out. */
	NTRY_I(KW_ON, ==, mdpp32qdc->mdpp.config.nim[i++]); /* Always TO. */
	NTRY_I(KW_OFF, ==, mdpp32qdc->mdpp.config.nim[i++]); /* Off. */
	NTRY_I(KW_OFF, ==, mdpp32qdc->mdpp.config.nim[i++]); /* Off. */
	NTRY_I(KW_TRIG0, ==, mdpp32qdc->mdpp.config.nim[i++]); /* T0. */

	i = 0;
	NTRY_I(KW_BUSY, ==, mdpp32qdc->mdpp.config.ecl[i++]); /* Busy out. */
	NTRY_I(KW_OFF, ==, mdpp32qdc->mdpp.config.ecl[i++]); /* Off. */
	NTRY_I(KW_OFF, ==, mdpp32qdc->mdpp.config.ecl[i++]); /* Off. */
	NTRY_I(KW_OFF, ==, mdpp32qdc->mdpp.config.ecl[i++]); /* Off. */

	NTRY_DBL(0, <=, mdpp32qdc->mdpp.config.gate_offset);
	NTRY_DBL(0, <=, mdpp32qdc->mdpp.config.gate_width);

	for (i = 0; 32 > i; ++i) {
		NTRY_I(0xff, ==, mdpp32qdc->mdpp.config.threshold[i]);
	}

	crate_free(&crate);
}

NTEST_SUITE(MESYTEC_MDPP32QDC)
{
	crate_setup();
	module_setup();
	map_setup();

	NTEST_ADD(DefaultConfig);

	map_user_clear();
	map_shutdown();
	config_shutdown();
}
