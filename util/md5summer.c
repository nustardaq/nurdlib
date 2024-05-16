/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2024
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

#include <stdio.h>
#include <unistd.h>
#include <util/md5.h>

int
main(void)
{
	struct MD5Context md5;
	char buf[256];
	uint8_t sum[MD5_DIGEST_LENGTH];
	int i;

	MD5Init(&md5);
	for (;;) {
		ssize_t ret;

		ret = read(0, buf, 256);
		if (0 == ret) {
			break;
		}
		MD5Update(&md5, buf, ret);
	}
	MD5Final(sum, &md5);
	for (i = MD5_DIGEST_LENGTH - 4; MD5_DIGEST_LENGTH > i; ++i) {
		printf("%02x", sum[i]);
	}
	puts("");
	return 0;
}
