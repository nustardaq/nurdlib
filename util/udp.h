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

#ifndef UTIL_UDP_H
#define UTIL_UDP_H

#include <stdlib.h>
#include <util/funcattr.h>
#include <util/stdint.h>

enum {
	UDP_IPV4 = 0x0,
	UDP_IPV6 = 0x1
};

struct UDPAddress;
struct UDPClient;
struct UDPServer;
/*
 * Putting the buf last means you may override the buffer size if the network
 * can handle it. Put dgram.bytes = 0 if you want sizeof(buf), otherwise it's
 * used as the size of the buf.
 */
struct UDPDatagram {
	size_t	bytes;
	uint8_t	buf[512];
};

struct UDPAddress	*udp_address_create(unsigned, char const *, uint16_t)
	FUNC_NONNULL(());
void			udp_address_free(struct UDPAddress **)
	FUNC_NONNULL(());
char			*udp_address_gets(struct UDPAddress const *)
	FUNC_NONNULL(());
uint32_t		udp_address_getu32(struct UDPAddress const *)
	FUNC_NONNULL(());
struct UDPClient	*udp_client_create(unsigned, char const *, uint16_t)
	FUNC_NONNULL(()) FUNC_RETURNS;
void			udp_client_free(struct UDPClient **) FUNC_NONNULL(());
int			udp_client_receive(struct UDPClient const *, struct
    UDPDatagram *, double) FUNC_NONNULL(());
int			udp_client_send(struct UDPClient const *, struct
    UDPDatagram const *) FUNC_NONNULL(());
struct UDPServer	*udp_server_create(unsigned, uint16_t) FUNC_RETURNS;
void			udp_server_free(struct UDPServer **) FUNC_NONNULL(());
int			udp_server_receive(struct UDPServer const *, struct
    UDPAddress **, struct UDPDatagram *, double) FUNC_NONNULL((1, 3));
int			udp_server_send(struct UDPServer const *, struct
    UDPAddress const *, struct UDPDatagram const *) FUNC_NONNULL(());
int			udp_server_write(struct UDPServer const *, void const
    *, size_t) FUNC_NONNULL(());

#endif
