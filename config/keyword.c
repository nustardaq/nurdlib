/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2019, 2021, 2024
 * Bastian Löher
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

#include <nurdlib/keyword.h>
#include <stdlib.h>
#include <string.h>
#include <config/kwarray.h>
#include <nurdlib/log.h>
#include <util/assert.h>

enum Keyword
keyword_get_id(char const *a_name)
{
	unsigned l, r;

	l = 1;
	r = KW_THIS_IS_ALWAYS_LAST - 1;
	for (;;) {
		unsigned m;
		int cmp;

		if (l > r) {
			log_die(LOGL, "Invalid keyword \"%s\".", a_name);
		}
		ASSERT(unsigned, "u", l, <=, r);
		m = (l + r) / 2;
		cmp = strcmp(a_name, g_keyword_array[m]);
		if (0 == cmp) {
			return m;
		}
		if (cmp < 0) {
			r = m - 1;
		} else {
			l = m + 1;
		}
	}
}

char const *
keyword_get_string(enum Keyword a_id)
{
	size_t idx;

	idx = a_id;
	if (KW_THIS_IS_ALWAYS_LAST <= idx) {
		log_die(LOGL, "Invalid keyword ID %u.", a_id);
	}
	return g_keyword_array[idx];
}
