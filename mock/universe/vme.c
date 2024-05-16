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

#include <stdint.h>
#include <stdlib.h>
#include <vme/vme.h>
#include <vme/vme_api.h>

int vme_init(vme_bus_handle_t *a_a) { return 0; }
int vme_master_window_create(vme_bus_handle_t a_a, vme_master_handle_t *a_b,
    uint64_t a_c, int a_d, size_t a_e, int a_f, void *a_g) { return 0; }
volatile void *vme_master_window_map(vme_bus_handle_t a_a, vme_master_handle_t
    a_b, int a_c) { return NULL; }
void vme_master_window_unmap(vme_bus_handle_t a_a, vme_master_handle_t a_b) {}
void vme_master_window_release(vme_bus_handle_t a_a, vme_master_handle_t a_b)
{}
void vme_term(vme_bus_handle_t a_a) {}
