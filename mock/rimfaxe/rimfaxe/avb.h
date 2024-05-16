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

#ifndef MOCK_RIMFAXE_AVB_H
#define MOCK_RIMFAXE_AVB_H

enum AVB_AM {
	AM_A32_DATA,
	AM_A32_BLT,
	AM_A32_MBLT,
	AVB_MASK_NONE
};

struct avb {
	int	a;
};

typedef int avb_err_code;

int avb_init(struct avb **);
void avb_free(struct avb *);
void volatile *avb_map(struct avb *, uintptr_t, size_t, int, int);
void avb_unmap(struct avb *, void volatile *);
char const *avb_get_err_msg(struct avb *);

int avb_safe_read16(struct avb *, uintptr_t, uint16_t *);
int avb_safe_read32(struct avb *, uintptr_t, uint32_t *);

#endif
