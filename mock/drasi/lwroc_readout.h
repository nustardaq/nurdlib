/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024-2025
 * Hans Toshihide Törnqvist
 * Håkan T Johansson
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

#ifndef LWROC_READOUT_H
#define LWROC_READOUT_H

struct lwroc_readout_functions {
	void (*untriggered_loop)(int *);
};

/* Actually in lwroc_net_proto.h. */
#define LWROC_TRIVA_STATUS_RUN               0x52554e2e
#define LWROC_TRIVA_STATUS_STOP              0x53544f50

#endif
