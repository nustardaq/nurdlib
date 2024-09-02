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

#include <nconf/util/math.c>
#include <util/math.h>

#if NCONF_mMATH_LINK_bNOTHING
#elif NCONF_mMATH_LINK_bLM
/* NCONF_LIBS=-lm */
#endif

#if NCONFING_mMATH_LINK
#	define NCONF_TEST sin(M_LN2 * argc) < 2.0
#endif

int32_t
i32_round_double(double a_d)
{
	return 0.0 > a_d ? -floor(0.5 - a_d) : floor(0.5 + a_d);
}
