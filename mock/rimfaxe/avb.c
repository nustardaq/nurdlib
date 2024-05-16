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

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <rimfaxe/avb.h>
#include <rimfaxe/avb-ff.h>

int avb_init(struct avb **a_a) { return 0; }
void avb_free(struct avb *a_a) {}
void volatile *avb_map(struct avb *a_a, uintptr_t a_b, size_t a_c, int a_d,
    int a_e)
{
	return NULL;
}
void avb_unmap(struct avb *a_a, void volatile *a_b) {}
char const *avb_get_err_msg(struct avb *a_a) { return NULL; }

int avb_safe_read16(struct avb *a_a, uintptr_t a_b, uint16_t *a_c)
{
	return 0;
}
int avb_safe_read32(struct avb *a_a, uintptr_t a_b, uint32_t *a_c)
{
	return 0;
}

struct avb_ff *avb_ff_map(struct avb *a_a, uintptr_t a_b, size_t a_c, enum
    AVB_AM a_d, int a_e)
{
	return NULL;
}
void avb_ff_unmap(struct avb *a_a, struct avb_ff *a_b) {}
int avb_ff_set_mblt_swap(struct avb_ff *a_a, int a_b) { return 0; }
ssize_t avb_ff_read_to_berr(struct avb_ff *a_a, uint32_t a_b, size_t a_c, void
    *a_d)
{
	return 0;
}
ssize_t avb_ff_read(struct avb_ff *a_a, uint32_t a_b, size_t a_c, void *a_d)
{
	return 0;
}
