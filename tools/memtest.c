/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2017, 2019, 2024
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

#include <stdio.h>
#include <inttypes.h>
#include <nurdlib.h>
#include <nurdlib/crate.h>

int
main(int argc, char **argv)
{
	struct Crate *crate;
	uint32_t cycle;

	(void)argc;
	(void)argv;

	/* TODO: Optional argument to set the config file? */
	crate = nurdlib_setup(NULL, "nurdlib.cfg", NULL, NULL);
	for (cycle = 0;; ++cycle) {
		LOGF(info)(LOGL, "Memtest cycle: %u", cycle);
		crate_memtest(crate, 1);
	}
	nurdlib_shutdown(&crate);

	return 0;
}
