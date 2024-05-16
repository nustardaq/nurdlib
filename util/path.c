/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2021, 2024
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

#include <util/path.h>
#include <stdlib.h>
#include <util/string.h>

char *
basename_dup(char const *a_path)
{
	char *base;
	int i, len;

	len = strlen(a_path);
	i = len - 1;
	if (0 == len || '/' == a_path[i]) {
		return strdup("");
	}
	for (; '/' != a_path[i]; --i) {
		if (0 == i) {
			return strdup(a_path);
		}
	}
	len -= i;
	base = malloc(len);
	strlcpy(base, a_path + i + 1, len);
	return base;
}

char *
dirname_dup(char const *a_path)
{
	char *dir;
	int i, len;

	len = strlen(a_path);
	for (i = len - 1; '/' != a_path[i]; --i) {
		if (0 >= i) {
			return strdup(".");
		}
	}
	dir = malloc(i + 1);
	memcpy(dir, a_path, i);
	dir[i] = '\0';
	return dir;
}
