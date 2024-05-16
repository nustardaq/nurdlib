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

#ifndef MOCK_RIMFAXE_AVB_FF_H
#define MOCK_RIMFAXE_AVB_FF_H

enum {
	AVB_FF_MASK_BERR,
	AVB_FF_NOMASK
};

struct avb_ff {
	int	a;
};

struct avb_ff *avb_ff_map(struct avb *, uintptr_t, size_t, enum AVB_AM, int);
void avb_ff_unmap(struct avb *, struct avb_ff *);
int avb_ff_set_mblt_swap(struct avb_ff *, int);
ssize_t avb_ff_read_to_berr(struct avb_ff *, uint32_t, size_t, void *);
ssize_t avb_ff_read(struct avb_ff *, uint32_t, size_t, void *);

#endif
