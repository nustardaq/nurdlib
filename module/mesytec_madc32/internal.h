/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2019, 2023-2024
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

#ifndef MODULE_MESYTEC_MADC32_INTERNAL_H
#define MODULE_MESYTEC_MADC32_INTERNAL_H

#include <module/mesytec_mxdc32/internal.h>

struct MesytecMadc32Module {
	struct	MesytecMxdc32Module mxdc32;
	int	do_auto_pedestals;
};

#endif