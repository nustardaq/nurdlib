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

#ifndef MOCK_ETHERBONE_H
#define MOCK_ETHERBONE_H

typedef unsigned eb_address_t;
typedef unsigned eb_data_t;
typedef unsigned eb_format_t;
typedef int eb_status_t;
typedef unsigned eb_width_t;

typedef void *eb_device_t;
typedef void *eb_operation_t;
typedef void *eb_socket_t;
typedef void *eb_cycle_t;
typedef void *eb_user_data_t;

typedef void (*eb_callback_t)(eb_user_data_t, eb_device_t, eb_operation_t,
    eb_status_t);

#ifndef MAP_LOCKED
#	define MAP_LOCKED 0
#endif
#define EB_NULL 0
#define eb_block 0

enum {
	EB_ABI_CODE,
	EB_ADDR32,
	EB_BIG_ENDIAN,
	EB_DATA32,
	EB_OK
};

struct sdb_device {
	unsigned	abi_class;
	unsigned	abi_ver_major;
	unsigned	abi_ver_minor;
	unsigned	bus_specific;
	struct {
		unsigned	addr_first;
		unsigned	addr_last;
		struct {
			unsigned	vendor_id;
			unsigned	device_id;
			unsigned	version;
			unsigned	date;
			char	name[1];
			unsigned	record_type;
		} product;
	} sdb_component;
};

eb_status_t eb_cycle_close(eb_cycle_t);
eb_status_t eb_cycle_open(eb_device_t, eb_user_data_t, eb_callback_t,
    eb_cycle_t *);
void eb_cycle_read(eb_cycle_t, eb_address_t, eb_format_t, eb_data_t *);
void eb_cycle_write(eb_cycle_t, eb_address_t, eb_format_t, eb_data_t);

eb_status_t eb_device_close(eb_device_t);
eb_status_t eb_device_open(eb_socket_t, char const *, eb_width_t, int,
    eb_device_t *);
eb_status_t eb_device_read(eb_device_t, eb_address_t, eb_format_t, eb_data_t
    *, eb_user_data_t, eb_callback_t);
eb_status_t eb_device_write(eb_device_t, eb_address_t, eb_format_t, eb_data_t,
    eb_user_data_t, eb_callback_t);

eb_status_t eb_sdb_find_by_identity(eb_device_t, unsigned, unsigned, struct
    sdb_device *, int *);

eb_status_t eb_socket_close(eb_socket_t);
eb_status_t eb_socket_open(unsigned short, char const *, eb_width_t,
    eb_socket_t *);

char const *eb_status(eb_status_t);

#endif
