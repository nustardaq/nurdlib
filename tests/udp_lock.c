/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2024
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

#include <util/udp.h>
#include <util/time.h>
#include <nurdlib/base.h>
#include <math.h>
#include <tests/udp_lock.h>

struct UDPServer *
udp_lock_by_server_port(unsigned a_flags, uint16_t a_port)
{
	struct UDPServer *server;
	int i;

	/* With tests running in parallel, port may be in use.
	 * Try several times.
	 */
	for (i = 0; i < 100; i++) {
		server = udp_server_create(a_flags, a_port);
		if (NULL != server)
			return server;
		/* Do a somewhat random sleep up to 100 ms. */
		time_sleep(fmod(time_getd(),0.1));
	}

	log_warn(LOGL, "Gave up creating UDP server at port %d.", a_port);
	return NULL;
}

struct UDPServer *g_lock_port = NULL;
