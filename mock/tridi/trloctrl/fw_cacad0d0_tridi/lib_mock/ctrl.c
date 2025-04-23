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

#include <stdint.h>
#include <stdlib.h>
#include <../include/tridi_functions.h>

volatile tridi_opaque *tridi_setup_map_hardware(char const *a_a, int a_b, void
    **a_c)
{
	return NULL;
}
void tridi_unmap_hardware(void *a_a) {}

void tridi_setup_check_version(volatile tridi_opaque *a_a) {}
void tridi_setup_readout_control(volatile tridi_opaque *a_a, struct
    tridi_readout_control *a_b, uint32_t a_c, uint32_t a_d, uint32_t a_e,
    uint32_t a_f) {}
