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
#include <util/string.h>
#include <ntest/ntest.h>
#include <tests/udp_lock.h>

NTEST(ServerSetup)
{
	struct UDPServer *server;

	LOCK_PORT;

	server = udp_server_create(UDP_IPV4, 12346);
	NTRY_PTR(NULL, !=, server);
	udp_server_free(&server);
	NTRY_PTR(NULL, ==, server);

	UNLOCK_PORT;
}

NTEST(ClientSetup)
{
	struct UDPClient *client;

	LOCK_PORT;

	client = udp_client_create(UDP_IPV4, "127.0.0.1", 12347);
	NTRY_PTR(NULL, !=, client);
	udp_client_free(&client);
	NTRY_PTR(NULL, ==, client);

	UNLOCK_PORT;
}

NTEST(ServerClient)
{
#define STRING "WhereMyDataAt?"
	struct UDPAddress *address;
	struct UDPDatagram datagram;
	struct UDPServer *server;
	char *s;

	LOCK_PORT;

	/*
	 * NOTE:
	 * One cannot really test UDP, but 127.0.0.1 should be ok. IPv4-v6
	 * interoperability is tricky, let's assume v4, also makes little
	 * practical sense to ask server/client what they are since they
	 * should be on different hosts.
	 */

	s = (char *)datagram.buf;
	server = udp_server_create(UDP_IPV4, 12348);
	NTRY_PTR(NULL, !=, server);

	{
		struct UDPClient *client;

		client = udp_client_create(UDP_IPV4, "127.0.0.1", 12348);
		NTRY_PTR(NULL, != , client);
		strlcpy_(s, STRING, sizeof datagram.buf);
		datagram.bytes = sizeof(STRING);
		udp_client_send(client, &datagram);
		udp_client_free(&client);
	}

	datagram.bytes = 0;
	udp_server_receive(server, &address, &datagram, 0.1);
	NTRY_PTR(NULL, !=, address);
	NTRY_I(sizeof STRING, ==, datagram.bytes);
	NTRY_STR(STRING, ==, s);
	udp_address_free(&address);

	datagram.bytes = 0;
	udp_server_receive(server, &address, &datagram, 0.1);
	NTRY_PTR(NULL, ==, address);
	NTRY_I(0, ==, datagram.bytes);

	udp_server_free(&server);

	UNLOCK_PORT;
}

NTEST(ServerWriting)
{
	struct UDPAddress *address;
	struct UDPDatagram datagram;
	struct UDPServer *server;
	uint8_t value;

	LOCK_PORT;

	server = udp_server_create(UDP_IPV4, 12349);
	value = 2;
	udp_server_write(server, &value, sizeof value);
	datagram.bytes = 0;
	NTRY_BOOL(udp_server_receive(server, &address, &datagram, 1.0));
	NTRY_PTR(NULL, ==, address);
	NTRY_I(1, ==, datagram.bytes);
	NTRY_I(2, ==, datagram.buf[0]);
	udp_server_free(&server);

	UNLOCK_PORT;
}

NTEST_SUITE(UDP)
{
	NTEST_ADD(ServerSetup);
	NTEST_ADD(ClientSetup);
	NTEST_ADD(ServerClient);
	NTEST_ADD(ServerWriting);
}
