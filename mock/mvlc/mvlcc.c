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
#include <mvlcc_wrap.h>

int
mvlcc_connect(mvlcc_t a)
{
	(void)a;
}

void
mvlcc_disconnect(mvlcc_t a)
{
	(void)a;
}

char const *
mvlcc_strerror(int a)
{
	(void)a;
}

void
mvlcc_free_mvlc(mvlcc_t a)
{
	(void)a;
}

mvlcc_t
mvlcc_make_mvlc(void const *a)
{
	(void)a;
}

int
mvlcc_single_vme_read(mvlcc_t a, unsigned b, uint32_t *c, unsigned d, unsigned
    e)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	(void)e;
}

int
mvlcc_single_vme_write(mvlcc_t a, unsigned b, uint32_t c, unsigned d, unsigned
    e)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	(void)e;
}
