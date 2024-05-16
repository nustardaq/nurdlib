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
#include <nurdlib.h>
#include <nurdlib/base.h>
#include <nurdlib/crate.h>
#include <module/dummy/internal.h>
#include <module/dummy/offsets.h>
#include <module/map/map.h>

NTEST(Run)
{
	char mem[MAP_SIZE];
	char dst[0x1000];
	struct Crate *crate;
	struct CrateTag *tag;
	struct Module *dummy;
	unsigned evn;

	map_user_add(0x01000000, mem, sizeof mem);

	crate = nurdlib_setup(NULL, "tests/crate_dummy.cfg");

	tag = crate_get_tag_by_name(crate, NULL);

	/* Find the first dummy module in the crate. */
	dummy = crate_module_find(crate, KW_DUMMY, 0);

	for (evn = 0; evn < 1000; ++evn) {
		struct EventBuffer eb, eb_orig;
		uint32_t ret;

		crate_tag_counter_increase(crate, tag, 1);
		dummy_counter_increase(dummy, 1);

		ret = crate_readout_dt(crate);
		NTRY_U(0, ==, ret);

		NTRY_U(dummy->event_counter.value, ==, 1 + evn);

		/* Setup and backup event-buffer. */
		eb.bytes = sizeof dst;
		eb.ptr = dst;
		COPY(eb_orig, eb);

		ret = crate_readout(crate, &eb);
		NTRY_U(0, ==, ret);

		crate_readout_finalize(crate);

		/*
		 * Simple event-buffer verification, done also by
		 * crate_readout.
		 */
		EVENT_BUFFER_INVARIANT(eb, eb_orig);
	}

	nurdlib_shutdown(&crate);
}

NTEST_SUITE(DAQ)
{
	NTEST_ADD(Run);

	map_user_clear();
}
