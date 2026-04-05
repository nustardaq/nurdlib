/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016
 * Håkan T Johansson
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

#ifndef TESTS_UDP_LOCK_H
#define TESTS_UDP_LOCK_H

#include <util/udp.h>

struct UDPServer *
udp_lock_by_server_port(unsigned a_flags, uint16_t a_port);

/* In order that tests that run in parallel do not interfere with each
 * other (client by accident talking to server opened by other test),
 * they are run only one at a time.  Locking is achieved by holding a
 * separate UDP port.
 */
extern struct UDPServer *g_lock_port;
#define LOCK_PORT do { \
		g_lock_port = udp_lock_by_server_port(UDP_IPV4, 12345); \
	} while (0)
#define UNLOCK_PORT do { \
		udp_server_free(&g_lock_port); \
	} while (0)

#endif
