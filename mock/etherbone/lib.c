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

#include <etherbone.h>

eb_status_t eb_cycle_close(eb_cycle_t a_a) { return 0; }
eb_status_t eb_cycle_open(eb_device_t a_a, eb_user_data_t a_b, eb_callback_t
    a_c, eb_cycle_t *a_d) { return 0; }
void eb_cycle_read(eb_cycle_t a_a, eb_address_t a_b, eb_format_t a_c,
    eb_data_t *a_d) {}
void eb_cycle_write(eb_cycle_t a_a, eb_address_t a_b, eb_format_t a_c,
    eb_data_t a_d) {}

eb_status_t eb_device_close(eb_device_t a_a) { return 0; }
eb_status_t eb_device_open(eb_socket_t a_a, char const *a_b, eb_width_t a_c, int
    a_d, eb_device_t *a_e) { return 0; }
eb_status_t eb_device_read(eb_device_t a_a, eb_address_t a_b, eb_format_t a_c,
    eb_data_t *a_d, eb_user_data_t a_e, eb_callback_t a_f) { return 0; }
eb_status_t eb_device_write(eb_device_t a_a, eb_address_t a_b, eb_format_t
    a_c, eb_data_t a_d, eb_user_data_t a_e, eb_callback_t a_f) { return 0; }

eb_status_t eb_sdb_find_by_identity(eb_device_t a_a, unsigned a_b, unsigned
    a_c, struct sdb_device *a_d, int *a_e) { return 0; }

eb_status_t eb_socket_close(eb_socket_t a_a) { return 0; }
eb_status_t eb_socket_open(unsigned short a_a, char const *a_b, eb_width_t
    a_c, eb_socket_t *a_d) { return 0; }

char const *eb_status(eb_status_t a_a) { return 0; }
