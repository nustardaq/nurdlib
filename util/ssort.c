/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2024
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

#include <util/ssort.h>

void
shell_sort(uint16_t *a_array, size_t a_array_size)
{
	size_t const c_gaps[] = {701, 301, 132, 57, 23, 10, 4, 1};
	size_t k;

	for (k = 0;; ++k) {
		size_t gap, i;

		gap = c_gaps[k];
		for (i = gap; a_array_size > i; ++i) {
			size_t j;
			uint16_t tmp;

			tmp = a_array[i];
			for (j = i; j >= gap && a_array[j - gap] > tmp; j -=
			    gap) {
				a_array[j] = a_array[j - gap];
			}
			a_array[j] = tmp;
		}
		if (1 == gap) {
			break;
		}
	}
}
