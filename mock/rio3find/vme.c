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

#include <ces/vmelib.h>

intptr_t
find_controller(uintptr_t a_address, size_t a_bytes, unsigned a_a, unsigned
    a_b, unsigned a_c, struct pdparam_master const *a_param)
{
	(void)a_address;
	(void)a_bytes;
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_param;
	return 0;
}

void
return_controller(intptr_t a_a, size_t a_b)
{
	(void)a_a;
	(void)a_b;
}
