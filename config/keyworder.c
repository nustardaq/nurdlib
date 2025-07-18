/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2019, 2024
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <config/kwarray.h>

#define LENGTH(x) (sizeof x / sizeof *x)

int
main(void)
{
	unsigned i;

	printf("#ifndef BUILD_CONFIG_KEYWORD_H\n");
	printf("#define BUILD_CONFIG_KEYWORD_H\n");
	printf("enum Keyword {\n");
	for (i = 0; i < LENGTH(g_keyword_array); ++i) {
		char name[80];

		if (0 == i) {
			strcpy(name, "NONE");
		} else {
			char const *s_cur;
			size_t j;

			s_cur = g_keyword_array[i];
			if (i >= 2) {
				char const *s_prev;

				s_prev = g_keyword_array[i - 1];
				if (strcmp(s_prev, s_cur) >= 0) {
					fprintf(stderr, "%u=%s >= %u=%s.\n",
					    i - 1, s_prev, i, s_cur);
					fprintf(stderr, "Keywords must be "
					    "listed in alphalexical "
					    "order!\n");
					exit(EXIT_FAILURE);
				}
			}
			for (j = 0; '\0' != s_cur[j]; ++j) {
				assert(j < sizeof name - 1);
				name[j] = (char) toupper(s_cur[j]);
			}
			name[j] = '\0';
		}
		printf("\tKW_%s,\n", name);
	}
	printf("\tKW_THIS_IS_ALWAYS_LAST\n");
	printf("};\n");
	printf("#endif\n");
	return 0;
}
