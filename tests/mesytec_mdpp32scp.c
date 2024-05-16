/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
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
#include <module/mesytec_mdpp32scp/internal.h>
#include <module/mesytec_mdpp32scp/offsets.h>

NTEST(DefaultConfig)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecMdpp32scpModule *mdpp32scp;
	unsigned i;

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_mdpp32scp_empty.cfg");
	crate = crate_create();

	mdpp32scp = (void *)crate_module_find(crate, KW_MESYTEC_MDPP32SCP, 0);
	mdpp32scp->mdpp.mxdc32.do_sleep = 0;

	crate_init(crate);

	NTRY_I(KW_MESYTEC_MDPP32SCP, ==, mdpp32scp->mdpp.mxdc32.module.type);
	NTRY_I(190, ==, mdpp32scp->mdpp.mxdc32.module.event_max);

	NTRY_I(0, ==, mdpp32scp->config.resolution[0]);
	NTRY_I(0, ==, mdpp32scp->config.resolution[1]);

	NTRY_I(0, ==, mdpp32scp->mdpp.config.monitor_on);
	NTRY_I(0, ==, mdpp32scp->mdpp.config.monitor_channel);
	NTRY_I(0, ==, mdpp32scp->mdpp.config.monitor_wave);

	i = 0;
	NTRY_I(KW_BUSY, ==, mdpp32scp->mdpp.config.nim[i++]); /* Busy out. */
	NTRY_I(KW_ON, ==, mdpp32scp->mdpp.config.nim[i++]); /* Always TO. */
	NTRY_I(KW_OFF, ==, mdpp32scp->mdpp.config.nim[i++]); /* Off. */
	NTRY_I(KW_OFF, ==, mdpp32scp->mdpp.config.nim[i++]); /* Off. */
	NTRY_I(KW_TRIG0, ==, mdpp32scp->mdpp.config.nim[i++]); /* T0. */

	i = 0;
	NTRY_I(KW_BUSY, ==, mdpp32scp->mdpp.config.ecl[i++]); /* Busy out. */
	NTRY_I(KW_OFF, ==, mdpp32scp->mdpp.config.ecl[i++]); /* Off. */
	NTRY_I(KW_OFF, ==, mdpp32scp->mdpp.config.ecl[i++]); /* Off. */
	NTRY_I(KW_OFF, ==, mdpp32scp->mdpp.config.ecl[i++]); /* Off. */

	NTRY_DBL(0, <=, mdpp32scp->mdpp.config.gate_offset);
	NTRY_DBL(0, <=, mdpp32scp->mdpp.config.gate_width);

	for (i = 0; 32 > i; ++i) {
		NTRY_I(0, ==, mdpp32scp->mdpp.config.threshold[i]);
	}

	crate_free(&crate);
}

NTEST(MonitorCollision0)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecMdpp32scpModule *mdpp32scp;

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_mdpp32scp_mon0.cfg");
	crate = crate_create();
	mdpp32scp = (void *)crate_module_find(crate, KW_MESYTEC_MDPP32SCP, 0);
	mdpp32scp->mdpp.mxdc32.do_sleep = 0;
	NTRY_SIGNAL(crate_init(crate));
	crate_free(&crate);
}

NTEST(MonitorCollision1)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecMdpp32scpModule *mdpp32scp;

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_mdpp32scp_mon1.cfg");
	crate = crate_create();
	mdpp32scp = (void *)crate_module_find(crate, KW_MESYTEC_MDPP32SCP, 0);
	mdpp32scp->mdpp.mxdc32.do_sleep = 0;
	NTRY_SIGNAL(crate_init(crate));
	crate_free(&crate);
}

NTEST(MonitorCollision2)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecMdpp32scpModule *mdpp32scp;

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_mdpp32scp_mon2.cfg");
	crate = crate_create();
	mdpp32scp = (void *)crate_module_find(crate, KW_MESYTEC_MDPP32SCP, 0);
	mdpp32scp->mdpp.mxdc32.do_sleep = 0;
	NTRY_SIGNAL(crate_init(crate));
	crate_free(&crate);
}

NTEST(TriggerDupT0)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecMdpp32scpModule *mdpp32scp;

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_mdpp32scp_t0.cfg");
	crate = crate_create();
	mdpp32scp = (void *)crate_module_find(crate, KW_MESYTEC_MDPP32SCP, 0);
	mdpp32scp->mdpp.mxdc32.do_sleep = 0;
	NTRY_SIGNAL(crate_init(crate));
	crate_free(&crate);
}

NTEST(TriggerDupT1)
{
	char mem[MAP_SIZE];
	struct Crate *crate;
	struct MesytecMdpp32scpModule *mdpp32scp;

	ZERO(mem);
	map_user_add(0x01000000, mem, sizeof mem);

	config_load("tests/mesytec_mdpp32scp_t1.cfg");
	crate = crate_create();
	mdpp32scp = (void *)crate_module_find(crate, KW_MESYTEC_MDPP32SCP, 0);
	mdpp32scp->mdpp.mxdc32.do_sleep = 0;
	NTRY_SIGNAL(crate_init(crate));
	crate_free(&crate);
}

NTEST_SUITE(MESYTEC_MDPP32SCP)
{
	crate_setup();
	module_setup();
	map_setup();

	NTEST_ADD(DefaultConfig);
	NTEST_ADD(MonitorCollision0);
	NTEST_ADD(MonitorCollision1);
	NTEST_ADD(MonitorCollision2);
	NTEST_ADD(TriggerDupT0);
	NTEST_ADD(TriggerDupT1);

	map_user_clear();
	map_shutdown();
	config_shutdown();
}
