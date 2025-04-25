/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2025
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
#include <stdlib.h>
#include <CAENVMElib.h>

char const *CAENVME_DecodeError(CVErrorCodes a_a)
{
	return NULL;
}

CVErrorCodes CAENVME_Init(CVBoardTypes a_a, short a_b, short a_c, int32_t
    *a_d)
{
	return 0;
}
CVErrorCodes CAENVME_Init2(CVBoardTypes a_a, void const *a_b, short a_c,
    int32_t *a_d)
{
	return 0;
}
CVErrorCodes CAENVME_End(int32_t a_a)
{
	return 0;
}

CVErrorCodes CAENVME_ReadCycle(int32_t a_a, uint32_t a_b, void *a_c, int a_d,
    int a_e)
{
	return 0;
}
CVErrorCodes CAENVME_WriteCycle(int32_t a_a, uint32_t a_b, void const *a_c,
    int a_d, int a_e)
{
	return 0;
}

CVErrorCodes CAENVME_BLTReadCycle(int32_t a_a, uint32_t a_b, void *a_c, int
    a_d, int a_e, int a_f, int *a_g)
{
	return 0;
}
