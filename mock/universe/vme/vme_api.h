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

#ifndef MOCK_UNIVERSE_VME_API_H
#define MOCK_UNIVERSE_VME_API_H

int vme_init(vme_bus_handle_t *);
int vme_master_window_create(vme_bus_handle_t, vme_master_handle_t *,
    uint64_t, int, size_t, int, void *);
volatile void *vme_master_window_map(vme_bus_handle_t, vme_master_handle_t,
    int);
void vme_master_window_unmap(vme_bus_handle_t, vme_master_handle_t);
void vme_master_window_release(vme_bus_handle_t, vme_master_handle_t);
void vme_term(vme_bus_handle_t);

#endif
