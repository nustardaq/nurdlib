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
#include <../hwmap_mapvme.h>

typedef void (*hwmap_error_internal_func)(int, const char *, int, const char
    *, ...);
extern hwmap_error_internal_func _hwmap_error_internal;

volatile hwmap_opaque *hwmap_map_vme(char const *a_a, uint32_t a_b, uint32_t
    a_c, const char *a_d, void **a_e)
{
	_hwmap_error_internal(0, NULL, 0, "");
	return NULL;
}
void hwmap_unmap_vme(void *a_a) {}
