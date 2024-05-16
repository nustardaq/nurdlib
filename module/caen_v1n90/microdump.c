/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2020-2021, 2023-2024
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

#include <module/caen_v1n90/micro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <module/caen_v1n90/internal.h>
#include <module/caen_v1n90/micro.h>
#include <module/map/map.h>


int
main(int argc, char **argv)
{
	void volatile *p;
	struct CaenV1n90Module module;
	struct Map *mapper;
	uint32_t addr;
	struct ModuleList module_list;
	int skip;

	if (2 != argc || 0 != strncmp("0x", argv[1], 2)) {
		fprintf(stderr, "Usage: %s 0x<addr>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	addr = strtol(argv[1] + 2, NULL, 16);
	printf("Address=0x%08x\n", addr);
	map_setup();
	mapper = map_map(addr, MAP_SIZE_MAX(*module),
	    KW_NOBLT, 0,
	    MAP_POKE_ARGS(*module->read, testreg),
	    MAP_POKE_ARGS(*module->write, testreg));

	TAILQ_INIT(&module_list);
	TAILQ_INSERT_TAIL(&module_list, (struct Module*) &module, next);

	{
		uint16_t acquisition;

		micro_write(&module_list, 0x0200, &skip);
		acquisition = micro_read(&module, &skip);
		printf("Acquisition mode= 0x%04x = %s.\n", acquisition, 0 == (0x1 &
		    acquisition) ? "Continuous" : "Trigger matching");
	}
	{
		uint16_t width;
		uint16_t offset;
		uint16_t extra;
		uint16_t reject;
		uint16_t subtraction;

		micro_write(&module_list, 0x1600, &skip);
		width = micro_read(&module, &skip);
		offset = micro_read(&module, &skip);
		extra = micro_read(&module, &skip);
		reject = micro_read(&module, &skip);
		subtraction = micro_read(&module, &skip);
		printf("Trigger configuration={width=0x%04x,", width);
		printf(" offset=0x%04x,extra=0x%04x, reject=0x%04x,", offset,
		    extra, reject);
		printf(" subtraction=0x%04x}.\n", subtraction);
	}
	{
		char const *str;
		uint16_t edge;

		micro_write(&module_list, 0x2300, &skip);
		edge = micro_read(&module, &skip);
		switch (edge) {
		case 0: str = "Pair"; break;
		case 1: str = "Trailing"; break;
		case 2: str = "Leading"; break;
		case 3: str = "Leading+Trailing"; break;
		default:
			log_error(VLERR, "Invalid edge detection=0x%04x.", edge);
			abort();
		}
		printf("Edge detection=0x%04x=%s.\n", edge, str);
	}
	{
		uint16_t resolution;
		int resolution_ps;

		micro_write(&module_list, 0x2600, &skip);
		resolution = micro_read(&module, &skip);
		switch (resolution) {
		case 0: resolution_ps = 800; break;
		case 1: resolution_ps = 200; break;
		case 2: resolution_ps = 100; break;
		case 3: resolution_ps = 25; break;
		default:
			log_error(VLERR, "Invalid resolution=0x%04x.",
			    resolution);
			abort();
		}
		printf("Resolution=0x%04x=%dps.\n", resolution, resolution_ps);
	}
	{
		uint16_t deadtime;
		int deadtime_ns;

		micro_write(&module_list, 0x2900, &skip);
		deadtime = micro_read(&module, &skip);
		switch (0x3 & deadtime) {
		case 0: deadtime_ns = 5; break;
		case 1: deadtime_ns = 10; break;
		case 2: deadtime_ns = 30; break;
		case 3: deadtime_ns = 100; break;
		default:
			abort();
		}
		printf("Deadtime=0x%04x=%dns", deadtime, deadtime_ns);
	}
	{
		uint16_t has;

		micro_write(&module_list, 0x3200, &skip);
		has = micro_read(&module, &skip);
		printf("Header+trailer=0x%04x=%s.\n", has, 0 == (0x1 & has) ?
		    "disabled" : "enabled");
	}
	map_unmap(&mapper);
	map_deinit();
	map_shutdown();
	return 0;
}
