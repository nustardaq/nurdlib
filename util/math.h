/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2021-2022, 2024
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

#ifndef UTIL_MATH_H
#define UTIL_MATH_H

#include <math.h>
#include <util/funcattr.h>
#include <util/stdint.h>

#ifndef M_LN2
#	define M_LN2 0.69314718055994530942
#endif

uint16_t	double2half(double) FUNC_RETURNS;
double		half2double(uint16_t) FUNC_RETURNS;
int32_t		i32_round_double(double) FUNC_RETURNS;

#endif
